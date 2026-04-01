// Fill out your copyright notice in the Description page of Project Settings.


#include "CS_Library.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FConstBuffer, "Constants");
IMPLEMENT_GLOBAL_SHADER(FClassify, "/MorphingMeshShaders/MC_Classify.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FEmit, "/MorphingMeshShaders/MC_Emit.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FDecoupledScan, "/MorphingMeshShaders/DecoupledScan.usf", "MainCS", SF_Compute);
