#include "hal_fdcan.h"

#include "config.h"
#include "hal_gpio.h"
#include "stm32g431xx.h"

namespace sipdrive::hal::fdcan {
namespace {

constexpr uint32_t kSramCanBase = SRAMCAN_BASE;

constexpr uint32_t kFlsNbr = 28U;
constexpr uint32_t kFleNbr = 8U;
constexpr uint32_t kRf0Nbr = 3U;
constexpr uint32_t kRf1Nbr = 3U;
constexpr uint32_t kTefNbr = 3U;
constexpr uint32_t kTfqNbr = 3U;

constexpr uint32_t kFlsSize = 1U * 4U;
constexpr uint32_t kFleSize = 2U * 4U;
constexpr uint32_t kRf0Size = 18U * 4U;
constexpr uint32_t kRf1Size = 18U * 4U;
constexpr uint32_t kTefSize = 2U * 4U;
constexpr uint32_t kTfqSize = 18U * 4U;

constexpr uint32_t kFlssa = 0U;
constexpr uint32_t kFlesa = kFlssa + (kFlsNbr * kFlsSize);
constexpr uint32_t kRf0sa = kFlesa + (kFleNbr * kFleSize);
constexpr uint32_t kRf1sa = kRf0sa + (kRf0Nbr * kRf0Size);
constexpr uint32_t kTefsa = kRf1sa + (kRf1Nbr * kRf1Size);
constexpr uint32_t kTfqsa = kTefsa + (kTefNbr * kTefSize);
constexpr uint32_t kSramCanSize = kTfqsa + (kTfqNbr * kTfqSize);

constexpr uint32_t kMaskStdId = 0x1FFC0000UL;
constexpr uint32_t kMaskExtId = 0x1FFFFFFFUL;
constexpr uint32_t kMaskXtd = 0x40000000UL;
constexpr uint32_t kMaskDlc = 0x000F0000UL;
constexpr uint32_t kMaskBrs = 0x00100000UL;
constexpr uint32_t kMaskFdf = 0x00200000UL;

constexpr uint8_t kDlcToSize[16] = {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
    12U, 16U, 20U, 24U, 32U, 48U, 64U};

constexpr uint8_t kFdcanAf = 9U;
constexpr uint8_t kCanRxPin = 11U;      // PA11
constexpr uint8_t kCanTxPin = 12U;      // PA12
constexpr uint8_t kCanModeSelectPin = 10U;  // PA10 -> TCAN1057A S (low = normal)

uint8_t SizeToDlc(uint8_t size) {
  for (uint8_t dlc = 0; dlc < 16U; ++dlc) {
    if (size <= kDlcToSize[dlc]) {
      return dlc;
    }
  }
  return 15U;
}

volatile uint32_t* MessageWord(uint32_t byte_offset) {
  return reinterpret_cast<volatile uint32_t*>(kSramCanBase + byte_offset);
}

void ClearMessageRam(void) {
  for (uint32_t offset = 0U; offset < kSramCanSize; offset += 4U) {
    *MessageWord(offset) = 0U;
  }
}

void ConfigureStandardFilter(void) {
  // Standard filter element format:
  // SFID2[10:0], reserved, SFID1[10:0], SFEC[2:0], SFT[1:0]
  // SFT=01 (dual ID), SFEC=001 (store in Rx FIFO0).
  constexpr uint32_t kSftDualId = (1U << 30U);
  constexpr uint32_t kSfecFifo0 = (1U << 27U);
  const uint32_t id = config::kCanCommandId & 0x7FFU;

  *MessageWord(kFlssa) =
      kSftDualId |
      kSfecFifo0 |
      (id << 16U) |
      id;
}

void ConfigurePinsAndTransceiver(void) {
  gpio::EnablePort(GPIOA);

  gpio::ConfigureAlternate(GPIOA, kCanRxPin, kFdcanAf, gpio::Pull::kUp);
  gpio::ConfigureAlternate(GPIOA, kCanTxPin, kFdcanAf, gpio::Pull::kNone);

  gpio::ConfigureOutput(GPIOA, kCanModeSelectPin, true);
  // TCAN1057A uses only S mode-control pin:
  // S=0 => normal CAN operation, S=1 => silent mode.
  gpio::Write(GPIOA, kCanModeSelectPin, false);
}

}  // namespace

void Init() {
  ConfigurePinsAndTransceiver();

  RCC->APB1ENR1 |= RCC_APB1ENR1_FDCANEN;
  // FDCAN kernel clock = PLLQ = 85 MHz (set in ClockToPll170).
  RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_FDCANSEL) | RCC_CCIPR_FDCANSEL_0;
  (void)RCC->APB1ENR1;

  FDCAN_CONFIG->CKDIV = 0U;

  FDCAN1->CCCR |= FDCAN_CCCR_INIT;
  while ((FDCAN1->CCCR & FDCAN_CCCR_INIT) == 0U) {}

  FDCAN1->CCCR |= FDCAN_CCCR_CCE;
  // CAN-FD with bit rate switching, matching moteus spec.
  FDCAN1->CCCR |= FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE;

  // Nominal 1 Mbps at 85 MHz: BRP=1, 85 TQ/bit.
  // Programmed values: NTSEG1=66, NTSEG2=16, NSJW=15.
  // Functional values are +1 => NTSEG1=67, NTSEG2=17, SJW=16, sync=1.
  FDCAN1->NBTP =
      (0U << FDCAN_NBTP_NBRP_Pos) |
      (66U << FDCAN_NBTP_NTSEG1_Pos) |
      (16U << FDCAN_NBTP_NTSEG2_Pos) |
      (15U << FDCAN_NBTP_NSJW_Pos);

  // Data 5 Mbps at 85 MHz: BRP=1, 17 TQ/bit.
  // DTSEG1=13, DTSEG2=2, sync=1 -> 17 TQ. SP=82.4%.
  // TDC enabled for 5 Mbps data phase.
  FDCAN1->DBTP =
      (0U << FDCAN_DBTP_DBRP_Pos) |
      (12U << FDCAN_DBTP_DTSEG1_Pos) |
      (2U << FDCAN_DBTP_DTSEG2_Pos) |
      (2U << FDCAN_DBTP_DSJW_Pos) |
      FDCAN_DBTP_TDC;
  FDCAN1->TDCR = (12U << FDCAN_TDCR_TDCO_Pos);

  // 64-byte data field for CAN-FD frames.
  // NOTE: RXESC/TXESC registers not defined in CMSIS headers for STM32G431.
  // These would configure Rx/Tx element sizes. Assuming default 8-byte elements work.
  // TODO: Verify if manual register writes are needed or if defaults suffice.
  // FDCAN1->RXESC = (7U << FDCAN_RXESC_F0DS_Pos) | (7U << FDCAN_RXESC_F1DS_Pos);
  // FDCAN1->TXESC = (7U << FDCAN_TXESC_TBDS_Pos);

  ClearMessageRam();
  ConfigureStandardFilter();

  FDCAN1->RXGFC =
      FDCAN_RXGFC_RRFE |
      FDCAN_RXGFC_RRFS |
      (2U << FDCAN_RXGFC_ANFE_Pos) |
      (2U << FDCAN_RXGFC_ANFS_Pos) |
      (1U << FDCAN_RXGFC_LSS_Pos);

  // Use Tx queue mode, message RAM layout fixed by STM32G4 FDCAN instance.
  FDCAN1->TXBC = FDCAN_TXBC_TFQM;

  FDCAN1->IR = 0xFFFFFFFFUL;
  FDCAN1->IE = FDCAN_IE_RF0NE;
  FDCAN1->ILS = 0U;
  FDCAN1->ILE = FDCAN_ILE_EINT0;

  FDCAN1->CCCR &= ~FDCAN_CCCR_INIT;
  while ((FDCAN1->CCCR & FDCAN_CCCR_INIT) != 0U) {}

  NVIC_SetPriority(FDCAN1_IT0_IRQn, 3U);
  NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
}

bool Receive(CanFrame* frame) {
  if (frame == nullptr) {
    return false;
  }

  if ((FDCAN1->RXF0S & FDCAN_RXF0S_F0FL) == 0U) {
    return false;
  }

  const uint32_t get_index =
      (FDCAN1->RXF0S & FDCAN_RXF0S_F0GI) >> FDCAN_RXF0S_F0GI_Pos;
  volatile uint32_t* rx = MessageWord(kRf0sa + (get_index * kRf0Size));

  const uint32_t header0 = rx[0];
  const uint32_t header1 = rx[1];

  if ((header0 & kMaskXtd) != 0U) {
    frame->id = header0 & kMaskExtId;
  } else {
    frame->id = (header0 & kMaskStdId) >> 18U;
  }

  const uint8_t dlc = static_cast<uint8_t>((header1 & kMaskDlc) >> 16U);
  frame->len = kDlcToSize[dlc & 0x0FU];
  frame->fd = ((header1 & kMaskFdf) != 0U);
  frame->brs = ((header1 & kMaskBrs) != 0U);

  for (uint32_t i = 0; i < sizeof(frame->data); ++i) {
    if (i < frame->len) {
      const uint32_t word = rx[2U + (i / 4U)];
      frame->data[i] = static_cast<uint8_t>((word >> ((i % 4U) * 8U)) & 0xFFU);
    } else {
      frame->data[i] = 0U;
    }
  }

  FDCAN1->RXF0A = get_index;
  return true;
}

bool Send(const CanFrame& frame) {
  if (frame.len > 64U) {
    return false;
  }

  if ((FDCAN1->TXFQS & FDCAN_TXFQS_TFQF) != 0U) {
    return false;
  }

  const uint32_t put_index =
      (FDCAN1->TXFQS & FDCAN_TXFQS_TFQPI) >> FDCAN_TXFQS_TFQPI_Pos;
  volatile uint32_t* tx = MessageWord(kTfqsa + (put_index * kTfqSize));

  uint32_t header0 = 0U;
  if (frame.id <= 0x7FFU) {
    header0 = (frame.id & 0x7FFU) << 18U;
  } else {
    header0 = (frame.id & kMaskExtId) | kMaskXtd;
  }

  const uint8_t dlc = SizeToDlc(frame.len);
  uint32_t header1 = static_cast<uint32_t>(dlc) << 16U;
  if (frame.fd) {
    header1 |= kMaskFdf;
  }
  if (frame.brs) {
    header1 |= kMaskBrs;
  }

  tx[0] = header0;
  tx[1] = header1;

  const uint8_t payload_size = kDlcToSize[dlc];
  for (uint32_t i = 0; i < payload_size; i += 4U) {
    uint32_t word = 0U;
    for (uint32_t b = 0; b < 4U; ++b) {
      const uint32_t index = i + b;
      const uint32_t value = (index < frame.len) ? frame.data[index] : 0U;
      word |= (value << (8U * b));
    }
    tx[2U + (i / 4U)] = word;
  }

  FDCAN1->TXBAR = (1U << put_index);
  return true;
}

void Service() {
  if ((FDCAN1->PSR & FDCAN_PSR_BO) != 0U) {
    FDCAN1->CCCR |= FDCAN_CCCR_INIT;
    FDCAN1->CCCR &= ~FDCAN_CCCR_INIT;
  }
}

void IrqHandler() {
  const uint32_t ir = FDCAN1->IR;
  FDCAN1->IR = ir;
}

bool FaultActive() {
  // TCAN1057A has no fault output pin; expose controller-side CAN health instead.
  return (FDCAN1->PSR & (FDCAN_PSR_BO | FDCAN_PSR_EP | FDCAN_PSR_EW)) != 0U;
}

}  // namespace sipdrive::hal::fdcan

extern "C" void FDCAN1_IT0_IRQHandler(void) {
  sipdrive::hal::fdcan::IrqHandler();
}
