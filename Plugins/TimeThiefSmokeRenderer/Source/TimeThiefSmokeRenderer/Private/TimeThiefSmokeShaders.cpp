#include "TimeThiefSmokeShaders.h"

IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeInitCS, "/TimeThiefSmokeShaders/TimeThiefSmokeInit.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeApplyEventsCS, "/TimeThiefSmokeShaders/TimeThiefSmokeApplyEvents.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeBulletSuppressCS, "/TimeThiefSmokeShaders/TimeThiefSmokeBulletSuppress.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeDynamicObstacleCS, "/TimeThiefSmokeShaders/TimeThiefSmokeDynamicObstacle.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeCarrierUpdateCS, "/TimeThiefSmokeShaders/TimeThiefSmokeCarrierUpdate.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeSimulateCS, "/TimeThiefSmokeShaders/TimeThiefSmokeSimulate.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeVorticityCS, "/TimeThiefSmokeShaders/TimeThiefSmokeVorticity.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeUpdateVortexParticlesCS, "/TimeThiefSmokeShaders/TimeThiefSmokeUpdateVortexParticles.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeSplatVortexParticlesCS, "/TimeThiefSmokeShaders/TimeThiefSmokeSplatVortexParticles.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeWarpCS, "/TimeThiefSmokeShaders/TimeThiefSmokeWarp.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeDivergenceCS, "/TimeThiefSmokeShaders/TimeThiefSmokeDivergence.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokePressureJacobiCS, "/TimeThiefSmokeShaders/TimeThiefSmokePressureJacobi.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeProjectVelocityCS, "/TimeThiefSmokeShaders/TimeThiefSmokeProjectVelocity.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTimeThiefSmokeCompositePS, "/TimeThiefSmokeShaders/TimeThiefSmokeComposite.usf", "MainPS", SF_Pixel);
