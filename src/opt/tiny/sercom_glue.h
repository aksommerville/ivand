/* C interfaces to SERCOM.h.
 * Ultimately my goal is to build for TinyArcade with no C++ linkage... this is as far as I've gotten.
 */

#ifndef SERCOM_GLUE_H
#define SERCOM_GLUE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void GluedSercom;

GluedSercom *sercom_getPERIPH_SPI1();
GluedSercom *sercom_getSercom3();

void sercom_disableSPI(GluedSercom *s);
void sercom_enableSPI(GluedSercom *s);
void sercom_enableWIRE(GluedSercom *s);
void sercom_initMasterWIRE(GluedSercom *s,uint32_t a);
void sercom_initSPI(GluedSercom *s,int a,int b,int c,int d);
void sercom_initSPIClock(GluedSercom *s,int a,uint32_t b);
uint8_t sercom_isAddressMatch(GluedSercom *s);
uint8_t sercom_isDataReadyWIRE(GluedSercom *s);
uint8_t sercom_isMasterReadOperationWIRE(GluedSercom *s);
uint8_t sercom_isRestartDetectedWIRE(GluedSercom *s);
uint8_t sercom_isSlaveWIRE(GluedSercom *s);
uint8_t sercom_isStopDetectedWIRE(GluedSercom *s);
void sercom_prepareAckBitWIRE(GluedSercom *s);
void sercom_prepareCommandBitsWire(GluedSercom *s,uint8_t a);
uint8_t sercom_sendDataMasterWIRE(GluedSercom *s,uint8_t a);
uint8_t sercom_sendDataSlaveWIRE(GluedSercom *s,uint8_t a);
void sercom_setBaudrateSPI(GluedSercom *s,uint8_t a);
void sercom_setClockModeSPI(GluedSercom *s,uint8_t a);
uint8_t sercom_startTransmissionWIRE(GluedSercom *s,uint8_t a,uint8_t b);
uint8_t sercom_transferDataSPI(GluedSercom *s,uint8_t a);

#ifdef __cplusplus
}
#endif

#endif
