#pragma once

#ifndef _SOLVER_Stereo_EQUATIONS_
#define _SOLVER_Stereo_EQUATIONS_

#include <cutil_inline.h>
#include <cutil_math.h>

#include "WarpingSolverUtil.h"
#include "WarpingSolverState.h"
#include "WarpingSolverParameters.h"

////////////////////////////////////////
// evalF
////////////////////////////////////////
__inline__ __device__ float evalFDevice(unsigned int variableIdx, SolverInput& input, SolverState& state, SolverParameters& parameters)
{
	float3 e = make_float3(0.0f, 0.0F, 0.0f);
	
	// E_fit
	float3 targetDepth = state.d_target[variableIdx];
	float3 e_fit = (state.d_x[variableIdx] - targetDepth);
	e += parameters.weightFitting * e_fit * e_fit;
	
	// E_reg
	int i; int j; get2DIdx(variableIdx, input.width, input.height, i, j);
	const int n0_i = i;		const int n0_j = j - 1; const bool validN0 = isInsideImage(n0_i, n0_j, input.width, input.height);
	const int n1_i = i;		const int n1_j = j + 1; const bool validN1 = isInsideImage(n1_i, n1_j, input.width, input.height);
	const int n2_i = i - 1; const int n2_j = j;		const bool validN2 = isInsideImage(n2_i, n2_j, input.width, input.height);
	const int n3_i = i + 1; const int n3_j = j;		const bool validN3 = isInsideImage(n3_i, n3_j, input.width, input.height);
	
	float3 p = state.d_x[get1DIdx(i, j, input.width, input.height)];
	float e_reg = 0.0f;
	if (validN0){ float3 q = state.d_x[get1DIdx(n0_i, n0_j, input.width, input.height)]; float3 v = (p - q); float norm = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); e_reg += norm; } // This is not really l_0, but l_0 is hard to threshold
	if (validN1){ float3 q = state.d_x[get1DIdx(n1_i, n1_j, input.width, input.height)]; float3 v = (p - q); float norm = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); e_reg += norm; }
	if (validN2){ float3 q = state.d_x[get1DIdx(n2_i, n2_j, input.width, input.height)]; float3 v = (p - q); float norm = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); e_reg += norm; }
	if (validN3){ float3 q = state.d_x[get1DIdx(n3_i, n3_j, input.width, input.height)]; float3 v = (p - q); float norm = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); e_reg += norm; }
	
	float res = e.x + e.y + e.z + parameters.weightRegularizer*e_reg;
	
	return res;
}

////////////////////////////////////////
// applyJT : this function is called per variable and evaluates each residual influencing that variable (i.e., each energy term per variable)
////////////////////////////////////////

__inline__ __device__ float3 evalMinusJTFDevice(unsigned int variableIdx, mat3x1* aux, SolverInput& input, SolverState& state, SolverParameters& parameters)
{
	state.d_delta[variableIdx] = make_float3(0.0f, 0.0f, 0.0f);

	float3 b   = make_float3(0.0f, 0.0f, 0.0f);
	float3 pre = make_float3(0.0f, 0.0f, 0.0f);

	int i; int j; get2DIdx(variableIdx, input.width, input.height, i, j);
	const int n0_i = i;		const int n0_j = j - 1; const bool validN0 = isInsideImage(n0_i, n0_j, input.width, input.height);
	const int n1_i = i;		const int n1_j = j + 1; const bool validN1 = isInsideImage(n1_i, n1_j, input.width, input.height);
	const int n2_i = i - 1; const int n2_j = j;		const bool validN2 = isInsideImage(n2_i, n2_j, input.width, input.height);
	const int n3_i = i + 1; const int n3_j = j;		const bool validN3 = isInsideImage(n3_i, n3_j, input.width, input.height);

	// fit/pos
	b += -2.0f*parameters.weightFitting*(state.d_x[variableIdx] - state.d_target[variableIdx]);
	pre += 2.0f*parameters.weightFitting;

	// reg/pos
	float3 p = state.d_x[get1DIdx(i, j, input.width, input.height)];
	float3 e_reg = make_float3(0.0f, 0.0f, 0.0f);
	if (validN0){ float3 q = state.d_x[get1DIdx(n0_i, n0_j, input.width, input.height)]; e_reg += 2.0f*(p - q - float3(aux[0])); pre += 4.0f*parameters.weightBeta*make_float3(1.0f, 1.0f, 1.0f); }
	if (validN1){ float3 q = state.d_x[get1DIdx(n1_i, n1_j, input.width, input.height)]; e_reg += 2.0f*(p - q - float3(aux[1])); pre += 4.0f*parameters.weightBeta*make_float3(1.0f, 1.0f, 1.0f); }
	if (validN2){ float3 q = state.d_x[get1DIdx(n2_i, n2_j, input.width, input.height)]; e_reg += 2.0f*(p - q - float3(aux[2])); pre += 4.0f*parameters.weightBeta*make_float3(1.0f, 1.0f, 1.0f); }
	if (validN3){ float3 q = state.d_x[get1DIdx(n3_i, n3_j, input.width, input.height)]; e_reg += 2.0f*(p - q - float3(aux[3])); pre += 4.0f*parameters.weightBeta*make_float3(1.0f, 1.0f, 1.0f); }
	b += -2.0f*parameters.weightBeta*e_reg;

	// Preconditioner
	if (pre.x > FLOAT_EPSILON) pre = 1.0f / pre;
	else					   pre = make_float3(1.0f, 1.0f, 1.0f);
	state.d_precondioner[variableIdx] = pre;

	return b;
}

////////////////////////////////////////
// applyJTJ : this function is called per variable and evaluates each residual influencing that variable (i.e., each energy term per variable)
////////////////////////////////////////

__inline__ __device__ float3 applyJTJDevice(unsigned int variableIdx, SolverInput& input, SolverState& state, SolverParameters& parameters)
{
	float3 b = make_float3(0.0f, 0.0f, 0.0f);

	int i; int j; get2DIdx(variableIdx, input.width, input.height, i, j);
	const int n0_i = i;		const int n0_j = j - 1; const bool validN0 = isInsideImage(n0_i, n0_j, input.width, input.height);
	const int n1_i = i;		const int n1_j = j + 1; const bool validN1 = isInsideImage(n1_i, n1_j, input.width, input.height);
	const int n2_i = i - 1; const int n2_j = j;		const bool validN2 = isInsideImage(n2_i, n2_j, input.width, input.height);
	const int n3_i = i + 1; const int n3_j = j;		const bool validN3 = isInsideImage(n3_i, n3_j, input.width, input.height);

	float3 p = state.d_p[get1DIdx(i, j, input.width, input.height)];
	float3 xp = state.d_x[get1DIdx(i, j, input.width, input.height)];
	
	// fit/pos
	b += 2.0f*parameters.weightFitting*state.d_p[variableIdx];

	// pos/reg
	float3 e_reg = make_float3(0.0f, 0.0f, 0.0f);
	if (validN0){ float3 xq = state.d_x[get1DIdx(n0_i, n0_j, input.width, input.height)]; e_reg += 2.0f*(p - state.d_p[get1DIdx(n0_i, n0_j, input.width, input.height)]); }
	if (validN1){ float3 xq = state.d_x[get1DIdx(n1_i, n1_j, input.width, input.height)]; e_reg += 2.0f*(p - state.d_p[get1DIdx(n1_i, n1_j, input.width, input.height)]); }
	if (validN2){ float3 xq = state.d_x[get1DIdx(n2_i, n2_j, input.width, input.height)]; e_reg += 2.0f*(p - state.d_p[get1DIdx(n2_i, n2_j, input.width, input.height)]); }
	if (validN3){ float3 xq = state.d_x[get1DIdx(n3_i, n3_j, input.width, input.height)]; e_reg += 2.0f*(p - state.d_p[get1DIdx(n3_i, n3_j, input.width, input.height)]); }
	b += 2.0f*parameters.weightBeta*e_reg;

	return b;
}

#endif