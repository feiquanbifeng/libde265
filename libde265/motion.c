/*
 * H.265 video codec.
 * Copyright (c) 2013 StrukturAG, Dirk Farin, <farin@struktur.de>
 *
 * This file is part of libde265.
 *
 * libde265 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libde265 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libde265.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "motion.h"
#include "motion_func.h"
#include "decctx.h"
#include "util.h"
#include <assert.h>


#include <sys/types.h>
#include <signal.h>
#include <string.h>


enum {
  // important! order like shown in 8.5.3.1.1
  PRED_A1  = 0,
  PRED_B1  = 1,
  PRED_B0  = 2,
  PRED_A0  = 3,
  PRED_B2  = 4,
  PRED_COL = 5,
  PRED_ZERO= 6
};


typedef struct
{
  uint8_t available[7];
  PredVectorInfo pred_vector[7];
} MergingCandidates;


void reset_pred_vector(PredVectorInfo* pvec)
{
  for (int X=0;X<2;X++) {
    pvec->mv[X].x = 0;
    pvec->mv[X].y = 0;
    pvec->refIdx[X] = -1;
    pvec->predFlag[X] = 0;
  }
}


void generate_inter_prediction_samples(decoder_context* ctx,
                                       int xC,int yC,
                                       int xB,int yB,
                                       int nCS, int nPbW,int nPbH,
                                       const VectorInfo* vi)
{
}


// 8.5.3.1.2
void derive_spatial_merging_candidates(const decoder_context* ctx,
                                       int xC, int yC, int nCS, int xP, int yP,
                                       uint8_t singleMCLFlag,
                                       int nPbW, int nPbH,
                                       int partIdx,
                                       MergingCandidates* out_cand)
{
  const pic_parameter_set* pps = ctx->current_pps;
  int log2_parallel_merge_level = pps->log2_parallel_merge_level;

  enum PartMode PartMode = get_PartMode(ctx,xC,yC);

  // --- A1 ---

  int xA1 = xP-1;
  int yA1 = yP+nPbH-1;

  bool availableA1;

  if (xP>>log2_parallel_merge_level == xA1>>log2_parallel_merge_level &&
      yP>>log2_parallel_merge_level == yA1>>log2_parallel_merge_level) {
    availableA1 = false;
  }
  else if (!singleMCLFlag &&
           partIdx==1 &&
           (PartMode==PART_Nx2N ||
            PartMode==PART_nLx2N ||
            PartMode==PART_nRx2N)) {
    availableA1 = false;
  }
  else {
    availableA1 = available_pred_blk(ctx, xC,yC, nCS, xP,yP, nPbW,nPbH,partIdx, xA1,yA1);
  }

  if (!availableA1) {
    out_cand->available[PRED_A1] = 0;
    reset_pred_vector(&out_cand->pred_vector[PRED_A1]);
  }
  else {
    out_cand->available[PRED_A1] = 1;
    out_cand->pred_vector[PRED_A1] = *get_mv_info(ctx,xA1,yA1);
  }

  // TODO...
  out_cand->available[PRED_A0] = 0;
  out_cand->available[PRED_B0] = 0;
  out_cand->available[PRED_B1] = 0;
  out_cand->available[PRED_B2] = 0;
  //assert(false);
}


// 8.5.3.1.4
void derive_zero_motion_vector_candidates(decoder_context* ctx,
                                          slice_segment_header* shdr,
                                          int* inout_mergeCandList,
                                          int* inout_numMergeCand,
                                          MergingCandidates* inout_mergeCand)
{
  int numRefIdx;

  if (shdr->slice_type==SLICE_TYPE_P) {
    numRefIdx = ctx->current_pps->num_ref_idx_l0_default_active;
  }
  else {
    numRefIdx = min(ctx->current_pps->num_ref_idx_l0_default_active,
                    ctx->current_pps->num_ref_idx_l1_default_active);
  }


  int numInputMergeCand = *inout_numMergeCand;
  int zeroIdx = 0;

  while (*inout_numMergeCand < shdr->MaxNumMergeCand) {
    // 1.

    PredVectorInfo* newCand = &inout_mergeCand->pred_vector[*inout_numMergeCand];

    if (shdr->slice_type==SLICE_TYPE_P) {
      newCand->refIdx[0] = (zeroIdx < numRefIdx) ? zeroIdx : 0;
      newCand->refIdx[1] = -1;
      newCand->predFlag[0] = 1;
      newCand->predFlag[1] = 0;
    }
    else {
      newCand->refIdx[0] = (zeroIdx < numRefIdx) ? zeroIdx : 0;
      newCand->refIdx[1] = (zeroIdx < numRefIdx) ? zeroIdx : 0;
      newCand->predFlag[0] = 1;
      newCand->predFlag[1] = 1;
    }

    newCand->mv[0].x = 0;
    newCand->mv[0].y = 0;
    newCand->mv[1].x = 0;
    newCand->mv[1].y = 0;

    (*inout_numMergeCand)++;

    // 2.

    zeroIdx++;
  }
}


// 8.5.3.1.7
void derive_temporal_luma_vector_prediction(decoder_context* ctx,
                                            int xP,int yP,
                                            int nPbW,int nPbH,
                                            int* refIdxCol,
                                            MotionVector* out_mvCol,
                                            uint8_t*      out_availableFlagCol)
{
}


// 8.5.3.1.1
void derive_luma_motion_merge_mode(decoder_context* ctx,
                                   int xC,int yC, int xP,int yP,
                                   int nCS, int nPbW,int nPbH, int partIdx,
                                   VectorInfo* out_vi)
{
  int singleMCLFlag;
  singleMCLFlag = (ctx->current_pps->log2_parallel_merge_level > 2 && nCS==8);

  if (singleMCLFlag) {
    xP=xC;
    yP=yC;
    nPbW=nCS;
    nPbH=nCS;
  }

  MergingCandidates mergeCand;
  derive_spatial_merging_candidates(ctx, xC,yC, nCS, xP,yP, singleMCLFlag,
                                    nPbW,nPbH,partIdx, &mergeCand);

  int refIdxCol[2] = { 0,0 };

  MotionVector mvCol[2];
  uint8_t availableFlagLCol[2];
  derive_temporal_luma_vector_prediction(ctx,xP,yP,nPbW,nPbH, refIdxCol, mvCol, availableFlagLCol);

  int availableFlagCol = availableFlagLCol[0] || availableFlagLCol[1];
  uint8_t* predFlagLCol = availableFlagLCol;

  // 4.

  int mergeCandList[7];
  int numMergeCand=0;

  for (int i=0;i<5;i++) {
    if (mergeCand.available[i]) {
      mergeCandList[numMergeCand++] = i;
    }
  }

  if (availableFlagCol) {
    mergeCandList[numMergeCand++] = PRED_COL;

    mergeCand.available[PRED_COL] = availableFlagCol;
    mergeCand.pred_vector[PRED_COL].mv[0] = mvCol[0];
    mergeCand.pred_vector[PRED_COL].mv[1] = mvCol[1];
    mergeCand.pred_vector[PRED_COL].predFlag[0] = predFlagLCol[0];
    mergeCand.pred_vector[PRED_COL].predFlag[1] = predFlagLCol[1];
    mergeCand.pred_vector[PRED_COL].refIdx[0] = refIdxCol[0];
    mergeCand.pred_vector[PRED_COL].refIdx[1] = refIdxCol[1];
  }

  // 5.

  int numOrigMergeCand = numMergeCand;

  // 6.

  int numCombMergeCand = 0; // TODO

  slice_segment_header* shdr = get_SliceHeader(ctx,xC,yC);
  if (shdr->slice_type == SLICE_TYPE_B) {
    assert(false); // TODO
  }


  // 7.

  derive_zero_motion_vector_candidates(ctx, shdr,
                                       mergeCandList, &numMergeCand, &mergeCand);

  // 8.

  int merge_idx = get_merge_idx(ctx,xP,yP);
  out_vi->lum = mergeCand.pred_vector[mergeCandList[merge_idx]];

  // 9.

  if (out_vi->lum.predFlag[0] && out_vi->lum.predFlag[1] && nPbW+nPbH==12) {
    out_vi->lum.refIdx[1] = -1;
    out_vi->lum.predFlag[1] = 0;
  }
}


// 8.5.3.1
void motion_vectors_indices(decoder_context* ctx,
                            int xC,int yC, int xB,int yB, int nCS, int nPbW,int nPbH, int partIdx,
                            VectorInfo* out_vi)
{
  int xP = xC+xB;
  int yP = yC+yB;

  if (get_pred_mode(ctx, xC,yC) == MODE_SKIP) {
    derive_luma_motion_merge_mode(ctx, xC,yC, xP,yP, nCS,nPbW,nPbH, partIdx, out_vi);
  }
}


// 8.5.3
void decode_prediction_unit(decoder_context* ctx,slice_segment_header* shdr,
                            int xC,int yC, int xB,int yB, int nCS, int nPbW,int nPbH, int partIdx)
{
  int nCS_L = nCS;
  int nCS_C = nCS>>1;

  // 1.

  VectorInfo vi;
  motion_vectors_indices(ctx, xC,yC, xB,yB, nCS, nPbW,nPbH, partIdx, &vi);

  // 2.

  generate_inter_prediction_samples(ctx, xC,yC, xB,yB, nCS, nPbW,nPbH, &vi);


  set_mv_info(ctx,xB,yB,nPbW,nPbH, &vi.lum);
}


// 8.5.2
void inter_prediction(decoder_context* ctx,slice_segment_header* shdr,
                      int xC,int yC, int log2CbSize)
{
  int nCS_L = 1<<log2CbSize;
  int nCS_C = nCS_L>>1;
  int nCS1L = nCS_L>>1;

  enum PartMode partMode = get_PartMode(ctx,xC,yC);
  switch (partMode) {
  case PART_2Nx2N:
    decode_prediction_unit(ctx,shdr,xC,yC, 0,0, nCS_L, nCS_L,nCS_L, 0);
    break;

  // ...

  default:
    assert(false); // TODO
  }
}