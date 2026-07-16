#include "TimeThiefSmokeShaders.h"

IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeTestReduceCS, "/TimeThiefSmokeShaders/TimeThiefSmokeTestReduce.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeInitCS, "/TimeThiefSmokeShaders/TimeThiefSmokeInit.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildObstacleFieldCS, "/TimeThiefSmokeShaders/TimeThiefSmokeUploadObstacleMask.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeApplyEventsCS, "/TimeThiefSmokeShaders/TimeThiefSmokeApplyEvents.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeDynamicObstacleCS, "/TimeThiefSmokeShaders/TimeThiefSmokeDynamicObstacle.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeSimulateCS, "/TimeThiefSmokeShaders/TimeThiefSmokeSimulate.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeVorticityCS, "/TimeThiefSmokeShaders/TimeThiefSmokeVorticity.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildCurlCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildCurl.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeUpdateVortexParticlesCS, "/TimeThiefSmokeShaders/TimeThiefSmokeUpdateVortexParticles.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildVortexBrickMasksCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildVortexBrickMasks.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeSplatVortexParticlesCS, "/TimeThiefSmokeShaders/TimeThiefSmokeSplatVortexParticles.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildBrickOccupancyCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildBrickOccupancy.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildEventBrickMasksCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildEventBrickMasks.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeExpandBrickOccupancyCS, "/TimeThiefSmokeShaders/TimeThiefSmokeExpandBrickOccupancy.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeScatterSparseAtlasCS, "/TimeThiefSmokeShaders/TimeThiefSmokeScatterSparseAtlas.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokePackDenseFieldCS, "/TimeThiefSmokeShaders/TimeThiefSmokePackDenseField.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildActiveBrickListCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildActiveBrickList.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildSparseScatterArgsCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildSparseScatterArgs.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeDivergenceCS, "/TimeThiefSmokeShaders/TimeThiefSmokeDivergence.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildMacDivergenceCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildMacDivergence.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokePressureJacobiCS, "/TimeThiefSmokeShaders/TimeThiefSmokePressureJacobi.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeProjectVelocityCS, "/TimeThiefSmokeShaders/TimeThiefSmokeProjectVelocity.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeProjectMacToCollocatedVelocityCS, "/TimeThiefSmokeShaders/TimeThiefSmokeProjectMacToCollocatedVelocity.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeCompositeMultiPS, "/TimeThiefSmokeShaders/TimeThiefSmokeCompositeMulti.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBilateralUpsamplePS, "/TimeThiefSmokeShaders/TimeThiefSmokeCompositeMulti.usf", "BilateralUpsampleMainPS", SF_Pixel);
