/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/

//============================================================
// include files
//============================================================

#include "mp_precomp.h"
#include "../phydm_precomp.h"

#if (RTL8814A_SUPPORT == 1)

VOID
odm_Write_Dynamic_CCA_8814A(
	IN	PDM_ODM_T		pDM_Odm,
	IN	u1Byte			CurrentMFstate
	)
{
	pPri_CCA_T		PrimaryCCA = &(pDM_Odm->DM_PriCCA);

	if (PrimaryCCA->MF_state != CurrentMFstate){

		ODM_SetBBReg(pDM_Odm, ODM_REG_L1SBD_PD_CH_11N, BIT8|BIT7, CurrentMFstate);
	}

	PrimaryCCA->MF_state = CurrentMFstate;

}

VOID
odm_PrimaryCCA_Check_Init_8814A(
	IN		PDM_ODM_T		pDM_Odm)
{
#if ((DM_ODM_SUPPORT_TYPE == ODM_WIN) || (DM_ODM_SUPPORT_TYPE == ODM_AP))
	PADAPTER		pAdapter = pDM_Odm->Adapter;
	pPri_CCA_T		PrimaryCCA = &(pDM_Odm->DM_PriCCA);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	pHalData->RTSEN = 0;
	PrimaryCCA->DupRTS_flag = 0;
	PrimaryCCA->intf_flag = 0;
	PrimaryCCA->intf_type = 0;
	PrimaryCCA->Monitor_flag = 0;
	PrimaryCCA->PriCCA_flag = 0;
	PrimaryCCA->CH_offset = 0;
	PrimaryCCA->MF_state = 0;
#endif /*((DM_ODM_SUPPORT_TYPE==ODM_WIN) ||(DM_ODM_SUPPORT_TYPE==ODM_AP)) */
}

VOID
odm_DynamicPrimaryCCA_Check_8814A(
	IN		PDM_ODM_T		pDM_Odm
	)
{
	if(pDM_Odm->SupportICType != ODM_RTL8814A)
		return;

	switch	(pDM_Odm->SupportPlatform)
	{
		case	ODM_WIN:

#if(DM_ODM_SUPPORT_TYPE==ODM_WIN)
			odm_DynamicPrimaryCCAMP_8814A(pDM_Odm);
#endif
			break;

		case	ODM_CE:
#if(DM_ODM_SUPPORT_TYPE==ODM_CE)

#endif
			break;

		case	ODM_AP:
#if (DM_ODM_SUPPORT_TYPE == ODM_AP)
			odm_DynamicPrimaryCCAAP_8814A(pDM_Odm);
#endif
			break;
		}

}


#if(DM_ODM_SUPPORT_TYPE==ODM_WIN)

VOID
odm_DynamicPrimaryCCAMP_8814A(
	IN		PDM_ODM_T		pDM_Odm
	)
{
	PADAPTER		pAdapter = pDM_Odm->Adapter;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	PFALSE_ALARM_STATISTICS		FalseAlmCnt = (PFALSE_ALARM_STATISTICS)PhyDM_Get_Structure( pDM_Odm, PHYDM_FALSEALMCNT);
	pPri_CCA_T		PrimaryCCA = &(pDM_Odm->DM_PriCCA);
	BOOLEAN			Is40MHz = FALSE;
	u8Byte			OFDM_CCA, OFDM_FA, BW_USC_Cnt, BW_LSC_Cnt;
	u1Byte			SecCHOffset;
	u1Byte			CurMFstate;
	static u1Byte		CountDown = Monitor_TIME;

	OFDM_CCA = FalseAlmCnt->Cnt_OFDM_CCA;
	OFDM_FA = FalseAlmCnt->Cnt_Ofdm_fail;
	BW_USC_Cnt = FalseAlmCnt->Cnt_BW_USC;
	BW_LSC_Cnt = FalseAlmCnt->Cnt_BW_LSC;
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_DYNAMIC_PRICCA, ODM_DBG_LOUD, ("8814A: OFDM CCA=%d\n", OFDM_CCA));
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_DYNAMIC_PRICCA, ODM_DBG_LOUD, ("8814A: OFDM FA=%d\n", OFDM_FA));
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_DYNAMIC_PRICCA, ODM_DBG_LOUD, ("8814A: BW_USC=%d\n", BW_USC_Cnt));
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_DYNAMIC_PRICCA, ODM_DBG_LOUD, ("8814A: BW_LSC=%d\n", BW_LSC_Cnt));
       Is40MHz = *(pDM_Odm->pBandWidth);
	SecCHOffset = *(pDM_Odm->pSecChOffset);		// NIC: 2: sec is below,  1: sec is above
	//DbgPrint("8814A: SecCHOffset = %d\n", SecCHOffset);
	if(!pDM_Odm->bLinked){
		return;
	}
	else{

		if(Is40MHz){
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_DYNAMIC_PRICCA, ODM_DBG_LOUD, ("8814A: Cont Down= %d\n", CountDown));
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_DYNAMIC_PRICCA, ODM_DBG_LOUD, ("8814A: Primary_CCA_flag=%d\n", PrimaryCCA->PriCCA_flag));
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_DYNAMIC_PRICCA, ODM_DBG_LOUD, ("8814A: Intf_Type=%d\n", PrimaryCCA->intf_type));
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_DYNAMIC_PRICCA, ODM_DBG_LOUD, ("8814A: Intf_flag=%d\n", PrimaryCCA->intf_flag ));
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_DYNAMIC_PRICCA, ODM_DBG_LOUD, ("8814A: Duplicate RTS Flag=%d\n", PrimaryCCA->DupRTS_flag));
			//DbgPrint("8814A RTS_EN=%d\n", pHalData->RTSEN);

			if(PrimaryCCA->PriCCA_flag == 0){

				if(SecCHOffset == 2){    // Primary channel is above   NOTE: duplicate CTS can remove this condition

					if((OFDM_CCA > OFDMCCA_TH) && (BW_LSC_Cnt>(BW_USC_Cnt + BW_Ind_Bias))
						&& (OFDM_FA>(OFDM_CCA>>1))){

						PrimaryCCA->intf_type = 1;
						PrimaryCCA->intf_flag = 1;
						CurMFstate = MF_USC;
						odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);
						PrimaryCCA->PriCCA_flag = 1;
					}
					else if((OFDM_CCA > OFDMCCA_TH) && (BW_LSC_Cnt>(BW_USC_Cnt + BW_Ind_Bias))
						&& (OFDM_FA < (OFDM_CCA>>1))){

 						PrimaryCCA->intf_type = 2;
						PrimaryCCA->intf_flag = 1;
						CurMFstate = MF_USC;
						odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);
						PrimaryCCA->PriCCA_flag = 1;
						PrimaryCCA->DupRTS_flag = 1;
						pHalData->RTSEN = 1;
					}
					else{

						PrimaryCCA->intf_type = 0;
						PrimaryCCA->intf_flag = 0;
						CurMFstate = MF_USC_LSC;
						odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);
						pHalData->RTSEN = 0;
						PrimaryCCA->DupRTS_flag = 0;
					}

				}
				else if (SecCHOffset == 1){

					if((OFDM_CCA > OFDMCCA_TH) && (BW_USC_Cnt > (BW_LSC_Cnt + BW_Ind_Bias))
						&& (OFDM_FA > (OFDM_CCA>>1))){

						PrimaryCCA->intf_type = 1;
						PrimaryCCA->intf_flag = 1;
						CurMFstate = MF_LSC;
						odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);
						PrimaryCCA->PriCCA_flag = 1;
					}
					else if((OFDM_CCA > OFDMCCA_TH) && (BW_USC_Cnt>(BW_LSC_Cnt + BW_Ind_Bias))
						&& (OFDM_FA < (OFDM_CCA>>1))){

 						PrimaryCCA->intf_type = 2;
						PrimaryCCA->intf_flag = 1;
						CurMFstate = MF_LSC;
						odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);
						PrimaryCCA->PriCCA_flag = 1;
						PrimaryCCA->DupRTS_flag = 1;
						pHalData->RTSEN = 1;
					}
					else{

						PrimaryCCA->intf_type = 0;
						PrimaryCCA->intf_flag = 0;
						CurMFstate = MF_USC_LSC;
						odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);
						pHalData->RTSEN = 0;
						PrimaryCCA->DupRTS_flag = 0;
					}

				}

			}
			else{	// PrimaryCCA->PriCCA_flag==1

				CountDown--;
				if(CountDown == 0){
					CountDown = Monitor_TIME;
					PrimaryCCA->PriCCA_flag = 0;
					CurMFstate = MF_USC_LSC;
					odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);   /* default*/
					pHalData->RTSEN = 0;
					PrimaryCCA->DupRTS_flag = 0;
					PrimaryCCA->intf_type = 0;
					PrimaryCCA->intf_flag = 0;
				}

			}

		}
		else{

			return;
		}
	}

}

#elif(DM_ODM_SUPPORT_TYPE == ODM_AP)

VOID
odm_DynamicPrimaryCCAAP_8814A(
	IN		PDM_ODM_T		pDM_Odm
	)
{
	PADAPTER	Adapter = pDM_Odm->Adapter;
	prtl8192cd_priv	priv = pDM_Odm->priv;
	PFALSE_ALARM_STATISTICS		FalseAlmCnt = (PFALSE_ALARM_STATISTICS)PhyDM_Get_Structure( pDM_Odm, PHYDM_FALSEALMCNT);
	pPri_CCA_T		PrimaryCCA = &(pDM_Odm->DM_PriCCA);

	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u1Byte 		i;
	static u4Byte	Count_Down = Monitor_TIME;
	BOOLEAN		STA_BW = FALSE, STA_BW_pre = FALSE, STA_BW_TMP = FALSE;
	BOOLEAN		bConnected = FALSE;
	BOOLEAN		Is40MHz = FALSE;
	u1Byte		SecCHOffset;
	u1Byte		CurMFstate;
	PSTA_INFO_T		pstat;

	Is40MHz = *(pDM_Odm->pBandWidth);
	SecCHOffset = *(pDM_Odm->pSecChOffset);		// AP: 1: sec is below,  2: sec is above


	for(i=0; i<ODM_ASSOCIATE_ENTRY_NUM; i++){
		pstat = pDM_Odm->pODM_StaInfo[i];
		if(IS_STA_VALID(pstat)){

			STA_BW_TMP = pstat->tx_bw;
			if(STA_BW_TMP > STA_BW){
				STA_BW = STA_BW_TMP;
			}
			bConnected = TRUE;
		}
	}

	if(Is40MHz){

		if(PrimaryCCA->PriCCA_flag == 0){

			if(bConnected){

				if(STA_BW == 0){   //2 STA BW=20M

					PrimaryCCA->PriCCA_flag = 1;
					if(SecCHOffset==1){
						CurMFstate = MF_USC;
						odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);
					}
					else if(SecCHOffset==2){
						CurMFstate = MF_USC;
						odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);
					}
				}
				else{     		 	//2  STA BW=40M
					if(PrimaryCCA->intf_flag == 0){

						odm_Intf_Detection(pDM_Odm);
					}
					else{	// intf_flag = 1

						if(PrimaryCCA->intf_type == 1){

							if(PrimaryCCA->CH_offset == 1){

								CurMFstate = MF_USC;
								if(SecCHOffset == 1){  // AP,  1: primary is above  2: primary is below
									odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);
								}
							}
							else if(PrimaryCCA->CH_offset == 2){

								CurMFstate = MF_LSC;
								if(SecCHOffset == 2){
									odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);
								}
							}
						}
						else if(PrimaryCCA->intf_type==2){

							if(PrimaryCCA->CH_offset==1){

								//ODM_SetBBReg(pDM_Odm, ODM_REG_L1SBD_PD_CH_11N, BIT8|BIT7, MF_USC);
								pHalData->RTSEN = 1;
							}
							else if(PrimaryCCA->CH_offset==2){

								//ODM_SetBBReg(pDM_Odm, ODM_REG_L1SBD_PD_CH_11N, BIT8|BIT7, MF_LSC);
								pHalData->RTSEN = 1;
							}

						}
					}
				}

			}
			else{		// disconnected  interference detection

				odm_Intf_Detection(pDM_Odm);
			}// end of disconnected


		}
		else{	// PrimaryCCA->PriCCA_flag == 1

			if(STA_BW==0){

				STA_BW_pre = STA_BW;
				return;
			}

			Count_Down--;
			if((Count_Down == 0) || ((STA_BW & STA_BW_pre) != 1)){

				Count_Down = Monitor_TIME;
				PrimaryCCA->PriCCA_flag = 0;
				PrimaryCCA->intf_type = 0;
				PrimaryCCA->intf_flag = 0;
				CurMFstate = MF_USC_LSC;
				odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate); /* default*/
				pHalData->RTSEN = 0;

			}

		}

		STA_BW_pre = STA_BW;

	}
	else{
		//2 Reset
		odm_PrimaryCCA_Check_Init(pDM_Odm);
		CurMFstate = MF_USC_LSC;
		odm_Write_Dynamic_CCA_8814A(pDM_Odm, CurMFstate);
		Count_Down = Monitor_TIME;
	}

}


VOID
odm_Intf_Detection_8814A(
	IN		PDM_ODM_T		pDM_Odm
	)
{
	PFALSE_ALARM_STATISTICS		FalseAlmCnt = (PFALSE_ALARM_STATISTICS)PhyDM_Get_Structure( pDM_Odm, PHYDM_FALSEALMCNT);
	pPri_CCA_T					PrimaryCCA = &(pDM_Odm->DM_PriCCA);

	if((FalseAlmCnt->Cnt_OFDM_CCA>OFDMCCA_TH)
		&&(FalseAlmCnt->Cnt_BW_LSC>(FalseAlmCnt->Cnt_BW_USC+BW_Ind_Bias))){

		PrimaryCCA->intf_flag = 1;
		PrimaryCCA->CH_offset = 1;  //  1:LSC, 2:USC
		if(FalseAlmCnt->Cnt_Ofdm_fail>(FalseAlmCnt->Cnt_OFDM_CCA>>1)){
			PrimaryCCA->intf_type = 1;
		}
		else{
			PrimaryCCA->intf_type = 2;
		}
	}
	else if((FalseAlmCnt->Cnt_OFDM_CCA>OFDMCCA_TH)
		&&(FalseAlmCnt->Cnt_BW_USC>(FalseAlmCnt->Cnt_BW_LSC+BW_Ind_Bias))){

		PrimaryCCA->intf_flag = 1;
		PrimaryCCA->CH_offset = 2;  //  1:LSC, 2:USC
		if(FalseAlmCnt->Cnt_Ofdm_fail>(FalseAlmCnt->Cnt_OFDM_CCA>>1)){
			PrimaryCCA->intf_type = 1;
		}
		else{
			PrimaryCCA->intf_type = 2;
		}
	}
	else{
		PrimaryCCA->intf_flag = 0;
		PrimaryCCA->intf_type = 0;
		PrimaryCCA->CH_offset = 0;
	}

}

#endif

u1Byte
phydm_spur_nbi_setting_8814a(
	IN		PDM_ODM_T		pDM_Odm
	)
{
	u1Byte	set_result = 0;

	/*pDM_Odm->pChannel means central frequency, so we can use 20M as input*/
	if (pDM_Odm->RFEType == 0 || pDM_Odm->RFEType == 1 || pDM_Odm->RFEType == 6) {
		/*channel asked by RF Jeff*/
		if (*pDM_Odm->pChannel == 14)
			set_result = phydm_nbi_setting(pDM_Odm,	NBI_ENABLE, *pDM_Odm->pChannel, 40, 2480, PHYDM_DONT_CARE);
		else if (*pDM_Odm->pChannel >= 4 || *pDM_Odm->pChannel <= 8)
			set_result = phydm_nbi_setting(pDM_Odm,	NBI_ENABLE, *pDM_Odm->pChannel, 40, 2440, PHYDM_DONT_CARE);
		else
			set_result = phydm_nbi_setting(pDM_Odm,	NBI_DISABLE, *pDM_Odm->pChannel, 40, 2440, PHYDM_DONT_CARE);
	}
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_COMMON, ODM_DBG_LOUD, ("%s, set_result = 0x%d, pChannel = %d\n", __func__, set_result, *pDM_Odm->pChannel));
	//printk("%s, set_result = 0x%d, pChannel = %d\n", __func__, set_result, *pDM_Odm->pChannel);
	pDM_Odm->nbi_set_result = set_result;
	return set_result;

}



#endif		// RTL8814A_SUPPORT == 1








