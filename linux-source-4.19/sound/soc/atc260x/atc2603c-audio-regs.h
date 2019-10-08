#ifndef _ATC2603C_AUDIO_REGS_H_
#define _ATC2603C_AUDIO_REGS_H_

/* ADC_DIGITALCTL */
#define ADC_DIGITALCTL_ADGC0            (((x) & 0xf) << 6)
#define ADC_DIGITALCTL_ADGC0_SFT        (6)

/* DAC_VOLUMECTL0 */
#define DAC_VOLUMECTL0_DACFL_VOLUME(x)  (((x) & 0xff) << 0)
#define DAC_VOLUMECTL0_DACFL_VOLUME_SFT (0)
#define DAC_VOLUMECTL0_DACFR_VOLUME(x)  (((x) & 0xff) << 8)
#define DAC_VOLUMECTL0_DACFR_VOLUME_SFT (8)

/* AGC_CTL0 */
#define AGC_CTL0_AMP0GR1(x)             (((x) & 0x7) << 0)
#define AGC_CTL0_AMP0GR1_SET            (0)
#define AGC_CTL0_IMICSHD                (0x1 << 7)
#define AGC_CTL0_AMP1G0R(x)             (((x) & 0xf) << 8)
#define AGC_CTL0_AMP1G0R_SFT            (8)
#define AGC_CTL0_AMP1G0L(x)             (((x) & 0xf) << 12)
#define AGC_CTL0_AMP1G0L_SFT            (12)
#define AGC_CTL0_AMP1GR1_MSK            (0x7 << 0)
#define AGC_CTL0_AMP1GR_MSK             (0xf << 8)
#define AGC_CTL0_AMP1GL_MSK             (0xf << 12)
#define AGC_CTL0_VMICINEN               (0x1 << 3)
#define AGC_CTL0_VMICINEN_SFT           (3)
#define AGC_CTL0_VMICEXST               (((x) & 0x3) << 4)
#define AGC_CTL0_VMICEXST_SFT           (4)
#define AGC_CTL0_VMICEXEN               (0x1 << 6)
#define AGC_CTL0_VMICEXEN_SFT           (6)

/* DAC_DIGITALCTL */
#define DAC_DIGITALCTL_DEFL             (0x1 << 0)
#define DAC_DIGITALCTL_DEFL_SFT         (0)
#define DAC_DIGITALCTL_DEFR             (0x1 << 1)
#define DAC_DIGITALCTL_DEFR_SFT         (1)
#define DAC_DIGITALCTL_DESW             (0x1 << 2)
#define DAC_DIGITALCTL_DEC              (0x1 << 3)
#define DAC_DIGITALCTL_DESL             (0x1 << 4)
#define DAC_DIGITALCTL_DESR             (0x1 << 5)
#define DAC_DIGITALCTL_DESBL            (0x1 << 6)
#define DAC_DIGITALCTL_DESBR            (0x1 << 7)
#define DAC_DIGITALCTL_DMFL             (0x1 << 8)
#define DAC_DIGITALCTL_DMFR             (0x1 << 9)
#define DAC_DIGITALCTL_DMSW             (0x1 << 10)
#define DAC_DIGITALCTL_DMC              (0x1 << 11)
#define DAC_DIGITALCTL_DMSL             (0x1 << 12)
#define DAC_DIGITALCTL_DMSR             (0x1 << 13)
#define DAC_DIGITALCTL_DMSBL            (0x1 << 14)
#define DAC_DIGITALCTL_DMSBR            (0x1 << 15)

/* DAC_ANALOG1 */
#define DAC_ANALOG1_VOLUME(x)           (((x) & 0x3f) << 0)
#define DAC_ANALOG1_VOLUME_SFT          (0)
#define DAC_ANALOG1_PASW                (0x1 << 6)
#define DAC_ANALOG1_PASW_SFT            (6)
#define DAC_ANALOG1_ZERODT              (0x1 << 7)
#define DAC_ANALOG1_PAIQ(x)             (((x) & 0x3) << 8)
#define DAC_ANALOG1_DACFL_FRMUTE        (0x1 << 10)
#define DAC_ANALOG1_DACFL_FRMUTE_SFT    (10)
#define DAC_ANALOG1_DACSW_CMUTE         (0x1 << 11)
#define DAC_ANALOG1_DACSL_SRMUTE        (0x1 << 12)
#define DAC_ANALOG1_DACSBL_SBRMUTE      (0x1 << 13)
#define DAC_ANALOG1_DACFMMUTE           (0x1 << 14)
#define DAC_ANALOG1_DACFMMUTE_SFT       (14)
#define DAC_ANALOG1_DACMICMUTE          (0x1 << 15)
#define DAC_ANALOG1_DACMICMUTE_SFT      (15)

/* DAC_ANALOG3 */
#define DAC_ANALOG3_DACEN_FR            (0x1)
#define DAC_ANALOG3_DACEN_FR_SFT        (0)
#define DAC_ANALOG3_DACEN_FL            (0x1 << 1)
#define DAC_ANALOG3_DACEN_FL_SFT        (1)
#define DAC_ANALOG3_PAEN_FR_FL          (0x1 << 2)
#define DAC_ANALOG3_PAEN_FR_FL_SFT      (2)
#define DAC_ANALOG3_PAOSEN_FR_FL        (0x1 << 3)
#define DAC_ANALOG3_PAOSEN_FR_FL_SFT    (3)

/* ADC_CTL */
#define ADC_CTL_ATAD_MTA_FTA_SFT        (0)
#define ADC_CTL_AD0REN                  (0x1 << 3)
#define ADC_CTL_AD0REN_SFT              (3)
#define ADC_CTL_AD0LEN                  (0x1 << 4)
#define ADC_CTL_AD0LEN_SFT              (4)
#define ADC_CTL_MIC0FDSE                (0x1 << 5)
#define ADC_CTL_MIC0FDSE_SFT            (5)
#define ADC_CTL_MIC0REN                 (0x1 << 6)
#define ADC_CTL_MIC0REN_SFT             (6)
#define ADC_CTL_MIC0LEN                 (0x1 << 7)
#define ADC_CTL_MIC0LEN_SFT             (7)
#define ADC_CTL_FMREN                   (0x1 << 13)
#define ADC_CTL_FMREN_SFT               (13)
#define ADC_CTL_FMLEN                   (0x1 << 14)
#define ADC_CTL_FMLEN_SFT               (14)

#endif

