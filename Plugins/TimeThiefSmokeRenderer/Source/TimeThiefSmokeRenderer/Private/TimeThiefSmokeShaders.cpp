#include "TimeThiefSmokeShaders.h"

IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeInitCS, "/TimeThiefSmokeShaders/TimeThiefSmokeInit.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeApplyEventsCS, "/TimeThiefSmokeShaders/TimeThiefSmokeApplyEvents.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeDynamicObstacleCS, "/TimeThiefSmokeShaders/TimeThiefSmokeDynamicObstacle.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeCarrierUpdateCS, "/TimeThiefSmokeShaders/TimeThiefSmokeCarrierUpdate.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildCarrierFieldCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildCarrierField.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeSimulateCS, "/TimeThiefSmokeShaders/TimeThiefSmokeSimulate.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeVorticityCS, "/TimeThiefSmokeShaders/TimeThiefSmokeVorticity.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildCurlCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildCurl.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeUpdateVortexParticlesCS, "/TimeThiefSmokeShaders/TimeThiefSmokeUpdateVortexParticles.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildVortexBrickMasksCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildVortexBrickMasks.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeSplatVortexParticlesCS, "/TimeThiefSmokeShaders/TimeThiefSmokeSplatVortexParticles.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildBrickOccupancyCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildBrickOccupancy.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeExpandBrickOccupancyCS, "/TimeThiefSmokeShaders/TimeThiefSmokeExpandBrickOccupancy.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeScatterSparseAtlasCS, "/TimeThiefSmokeShaders/TimeThiefSmokeScatterSparseAtlas.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeWarpCS, "/TimeThiefSmokeShaders/TimeThiefSmokeWarp.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeDivergenceCS, "/TimeThiefSmokeShaders/TimeThiefSmokeDivergence.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBuildMacVelocityCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBuildMacVelocity.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeMacDivergenceCS, "/TimeThiefSmokeShaders/TimeThiefSmokeMacDivergence.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokePressureJacobiCS, "/TimeThiefSmokeShaders/TimeThiefSmokePressureJacobi.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokePressureResidualCS, "/TimeThiefSmokeShaders/TimeThiefSmokePressureResidual.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokePressureRestrictCS, "/TimeThiefSmokeShaders/TimeThiefSmokePressureRestrict.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokePressureProlongateAddCS, "/TimeThiefSmokeShaders/TimeThiefSmokePressureProlongateAdd.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeProjectVelocityCS, "/TimeThiefSmokeShaders/TimeThiefSmokeProjectVelocity.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeProjectMacVelocityCS, "/TimeThiefSmokeShaders/TimeThiefSmokeProjectMacVelocity.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeMacToCollocatedVelocityCS, "/TimeThiefSmokeShaders/TimeThiefSmokeMacToCollocatedVelocity.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeCompositePS, "/TimeThiefSmokeShaders/TimeThiefSmokeComposite.usf", "MainPS", SF_Pixel);
