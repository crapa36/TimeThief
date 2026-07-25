#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re
import subprocess

BASE = "9af2a158b683877d79f623f9790a350701ba7941"
ROOT = Path(__file__).resolve().parents[2]

CPP = ROOT / "Plugins/TimeThiefSmokeRenderer/Source/TimeThiefSmokeRenderer/Private/TimeThiefSmokeViewExtension.cpp"
VIEW_H = ROOT / "Plugins/TimeThiefSmokeRenderer/Source/TimeThiefSmokeRenderer/Private/TimeThiefSmokeViewExtension.h"
SHADERS_CPP = ROOT / "Plugins/TimeThiefSmokeRenderer/Source/TimeThiefSmokeRenderer/Private/TimeThiefSmokeShaders.cpp"
SHADERS_H = ROOT / "Plugins/TimeThiefSmokeRenderer/Source/TimeThiefSmokeRenderer/Private/TimeThiefSmokeShaders.h"
MACROS_H = ROOT / "Plugins/TimeThiefSmokeRenderer/Source/TimeThiefSmokeRenderer/Private/TimeThiefSmokeShaderParameterMacros.h"
DEFAULTS_H = ROOT / "Plugins/TimeThiefSmokeRenderer/Source/TimeThiefSmokeRenderer/Public/TimeThiefSmokeParameterDefaults.h"
SHADER_DIR = ROOT / "Plugins/TimeThiefSmokeRenderer/Shaders"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def git_show(ref: str, path: Path) -> str:
    relative = path.relative_to(ROOT).as_posix()
    return subprocess.check_output(["git", "show", f"{ref}:{relative}"], cwd=ROOT, text=True)


def find_braced_span(text: str, marker: str) -> tuple[int, int]:
    pos = text.find(marker)
    if pos < 0:
        raise RuntimeError(f"Missing marker: {marker}")
    start = text.rfind("\n", 0, pos) + 1
    brace = text.find("{", pos)
    if brace < 0:
        raise RuntimeError(f"Missing opening brace after: {marker}")

    depth = 0
    i = brace
    in_string: str | None = None
    escaped = False
    line_comment = False
    block_comment = False
    while i < len(text):
        char = text[i]
        next_char = text[i + 1] if i + 1 < len(text) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
        elif block_comment:
            if char == "*" and next_char == "/":
                block_comment = False
                i += 1
        elif in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == in_string:
                in_string = None
        else:
            if char == "/" and next_char == "/":
                line_comment = True
                i += 1
            elif char == "/" and next_char == "*":
                block_comment = True
                i += 1
            elif char in ('"', "'"):
                in_string = char
            elif char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    return start, i + 1
        i += 1
    raise RuntimeError(f"Unclosed braced block: {marker}")


def extract_function(text: str, qualified_name: str) -> str:
    start, end = find_braced_span(text, qualified_name + "(")
    return text[start:end]


def replace_function(text: str, qualified_name: str, replacement: str) -> str:
    start, end = find_braced_span(text, qualified_name + "(")
    return text[:start] + replacement + text[end:]


def remove_function(text: str, qualified_name: str) -> str:
    start, end = find_braced_span(text, qualified_name + "(")
    while end < len(text) and text[end] in " \t":
        end += 1
    if end < len(text) and text[end] == "\n":
        end += 1
    if end < len(text) and text[end] == "\n":
        end += 1
    return text[:start] + text[end:]


def remove_class(text: str, class_name: str) -> str:
    start, end = find_braced_span(text, f"class {class_name} : public FGlobalShader")
    semicolon = text.find(";", end)
    if semicolon < 0:
        raise RuntimeError(f"Missing class semicolon: {class_name}")
    end = semicolon + 1
    while end < len(text) and text[end] in " \t":
        end += 1
    if end < len(text) and text[end] == "\n":
        end += 1
    if end < len(text) and text[end] == "\n":
        end += 1
    return text[:start] + text[end:]


def remove_console_variable(text: str, variable_name: str) -> str:
    pos = text.find(variable_name)
    if pos < 0:
        raise RuntimeError(f"Missing console variable: {variable_name}")
    start = text.rfind("\n", 0, pos) + 1
    end = text.find("\n\t);", pos)
    if end < 0:
        raise RuntimeError(f"Missing console variable terminator: {variable_name}")
    end += len("\n\t);")
    while end < len(text) and text[end] == "\n":
        end += 1
    return text[:start] + text[end:]


def remove_const_and_comment(text: str, name: str) -> str:
    lines = text.splitlines(keepends=True)
    matches = [index for index, line in enumerate(lines) if re.search(rf"\b{name}\b\s*=", line)]
    if len(matches) != 1:
        raise RuntimeError(f"Expected one constant {name}, found {len(matches)}")
    index = matches[0]
    start = index
    while start > 0 and lines[start - 1].lstrip().startswith("//"):
        start -= 1
    del lines[start:index + 1]
    return "".join(lines)


def replace_const_from_base(current: str, base: str, name: str) -> str:
    base_lines = [line for line in base.splitlines() if re.search(rf"\b{name}\b\s*=", line)]
    current_matches = list(re.finditer(rf"(?m)^.*\b{name}\b\s*=.*;$", current))
    if len(base_lines) != 1 or len(current_matches) != 1:
        raise RuntimeError(f"Cannot restore constant {name}: base={len(base_lines)}, current={len(current_matches)}")
    match = current_matches[0]
    return current[:match.start()] + base_lines[0] + current[match.end():]


def remove_lines_containing(text: str, tokens: tuple[str, ...]) -> str:
    return "".join(line for line in text.splitlines(keepends=True) if not any(token in line for token in tokens))


# Defaults: remove solver-selection/adaptive-readback settings, preserving all other tuning.
defaults = read(DEFAULTS_H)
for constant in (
    "QualityCourantLimit",
    "MaxAdaptiveFluidSubsteps",
    "PressureSolver",
    "MGPCGMaxIterations",
    "MGPCGPreSmoothIterations",
    "MGPCGPostSmoothIterations",
    "MGPCGCoarseIterations",
    "MGPCGRelativeTolerance",
    "VelocityReadbackIntervalFrames",
):
    defaults = remove_const_and_comment(defaults, constant)
defaults = defaults.replace("\t// 압력 솔버\n", "")
defaults = re.sub(r"\n{3,}", "\n\n", defaults)

base_defaults = git_show(BASE, DEFAULTS_H)
for constant in (
    "BulletClearRadius",
    "BulletClearRadiusRandomMin",
    "BulletClearRadiusRandomMax",
    "BulletWakeStrengthRandomMin",
    "BulletWakeStrengthRandomMax",
    "BulletWakeMaxVisibleLife",
    "BulletWakeReleaseDuration",
    "BulletWakeSinkLife",
    "BulletWakeSinkStrength",
    "BulletWakeImpulseStrength",
    "BulletWakeCutoutFeather",
    "BulletWakeMinLifeSeconds",
    "BulletWakeCutoutFeatherMin",
    "BulletWakeHoldCoreInnerRadiusScale",
    "BulletWakeHoldCoreOuterRadiusScale",
):
    defaults = replace_const_from_base(defaults, base_defaults, constant)
write(DEFAULTS_H, defaults)


# Shader registrations and parameter declarations.
shaders_cpp = read(SHADERS_CPP)
shaders_cpp = remove_lines_containing(shaders_cpp, ("FTimeThiefSmokeVelocityMax", "FTimeThiefSmokeMGPCG"))
write(SHADERS_CPP, shaders_cpp)

shaders_h = read(SHADERS_H)
for class_name in ("FTimeThiefSmokeVelocityMaxTilesCS", "FTimeThiefSmokeVelocityMaxFinalCS"):
    shaders_h = remove_class(shaders_h, class_name)
macro_start = shaders_h.find("#define TIME_THIEF_DECLARE_MGPCG_SHADER")
macro_end_marker = "#undef TIME_THIEF_DECLARE_MGPCG_SHADER"
macro_end = shaders_h.find(macro_end_marker, macro_start)
if macro_start < 0 or macro_end < 0:
    raise RuntimeError("Missing MGPCG shader declaration block")
macro_start = shaders_h.rfind("\n", 0, macro_start) + 1
macro_end += len(macro_end_marker)
while macro_end < len(shaders_h) and shaders_h[macro_end] == "\n":
    macro_end += 1
shaders_h = shaders_h[:macro_start] + shaders_h[macro_end:]
write(SHADERS_H, shaders_h)

macros_h = read(MACROS_H)
splat_start = macros_h.find("#define TIME_THIEF_SMOKE_SPLAT_VORTEX_PARTICLES_CS_PARAMETERS")
splat_end = macros_h.find("\n\n", splat_start)
if splat_start < 0 or splat_end < 0:
    raise RuntimeError("Missing vortex splat parameter macro")
splat_block = macros_h[splat_start:splat_end]
expected_delta_line = "\tTT_SMOKE_CPP_PARAM_DELTA_SECONDS \\\n"
if splat_block.count(expected_delta_line) != 1:
    raise RuntimeError("Unexpected vortex splat DeltaSeconds parameter layout")
splat_block = splat_block.replace(expected_delta_line, "")
macros_h = macros_h[:splat_start] + splat_block + macros_h[splat_end:]
write(MACROS_H, macros_h)


# View-extension declarations and state.
view_h = read(VIEW_H)
view_h = remove_lines_containing(
    view_h,
    (
        "VelocityMaximumReadback",
        "LastMeasuredMaxVelocity",
        "LastVelocityMeasurementFrame",
        "bVelocityMaximumReadbackPending",
        "ConsumeVelocityMaximumReadback",
        "QueueVelocityMaximumReadback",
        "AddMGPCGPressureSolvePasses",
    ),
)
old_sim_decl = "\tvoid SimulateSmoke(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, float DeltaSeconds, float EventDeltaSeconds, bool bFinalizeFluidStep, bool bIsFinalSimulationSubstep);"
new_sim_decl = "\tvoid SimulateSmoke(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, float DeltaSeconds, float EventDeltaSeconds, bool bIsFinalSimulationStep);"
if view_h.count(old_sim_decl) != 1:
    raise RuntimeError("Unexpected SimulateSmoke declaration")
view_h = view_h.replace(old_sim_decl, new_sim_decl)
old_splat_decl = "\tvoid AddVortexParticleSplatPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexBuffer, FRDGBufferRef VortexBrickMasksBuffer, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGTextureRef VelocityOut, float DeltaSeconds);"
new_splat_decl = "\tvoid AddVortexParticleSplatPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexBuffer, FRDGBufferRef VortexBrickMasksBuffer, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGTextureRef VelocityOut);"
if view_h.count(old_splat_decl) != 1:
    raise RuntimeError("Unexpected vortex splat declaration")
view_h = view_h.replace(old_splat_decl, new_splat_decl)
write(VIEW_H, view_h)


# View-extension implementation.
cpp = read(CPP)
for cvar in (
    "CVarTimeThiefSmokePressureSolver",
    "CVarTimeThiefSmokeMGPCGMaxIterations",
    "CVarTimeThiefSmokeMGPCGRelativeTolerance",
):
    cpp = remove_console_variable(cpp, cvar)
cpp = remove_function(cpp, "ComputeAdaptiveFluidSubstepCount")
cpp = remove_function(cpp, "FTimeThiefSmokeViewExtension::ConsumeVelocityMaximumReadback")
cpp = remove_function(cpp, "FTimeThiefSmokeViewExtension::QueueVelocityMaximumReadback")
cpp = remove_function(cpp, "FTimeThiefSmokeViewExtension::AddMGPCGPressureSolvePasses")
cpp = remove_lines_containing(
    cpp,
    (
        "Low Courant flow uses one fluid step",
        "High Courant flow clamps to the configured fluid budget",
        "MGPCG tolerance remains stricter than one percent",
        "State.VelocityMaximumReadback.Reset();",
        "State.bVelocityMaximumReadbackPending = false;",
        "State.LastMeasuredMaxVelocity = 0.0f;",
        "ConsumeVelocityMaximumReadback(State);",
        "WarmupComputeShader(TShaderMapRef<FTimeThiefSmokeVelocityMax",
        "WarmupComputeShader(TShaderMapRef<FTimeThiefSmokeMGPCG",
    ),
)

simulate = extract_function(cpp, "FTimeThiefSmokeViewExtension::SimulateSmoke")
old_signature = "\tfloat EventDeltaSeconds,\n\tbool bFinalizeFluidStep,\n\tbool bIsFinalSimulationSubstep)"
new_signature = "\tfloat EventDeltaSeconds,\n\tbool bIsFinalSimulationStep)"
if simulate.count(old_signature) != 1:
    raise RuntimeError("Unexpected SimulateSmoke definition signature")
simulate = simulate.replace(old_signature, new_signature)
simulate = simulate.replace("\t\tbIsFinalSimulationSubstep);", "\t\tbIsFinalSimulationStep);")

solver_start = simulate.find("\tFRDGTextureRef PressureForProjection = PressureTextures[0];")
solver_end = simulate.find("\tif (bCollectProjectionDiagnostics)", solver_start)
if solver_start < 0 or solver_end < 0:
    raise RuntimeError("Missing pressure solver selection block")
jacobi_block = """\tconst FVector3f PressureCellSize = MakeCellSize(State.Volume, State.AllocatedGridSize);\n\tint32 CurrentPressureIndex = 0;\n\tconst int32 PressureIterations = FMath::Clamp(\n\t\tTimeThiefSmokeParameterDefaults::PressureIterations,\n\t\tTimeThiefSmokeParameterDefaults::PressureIterationsMin,\n\t\tTimeThiefSmokeParameterDefaults::PressureIterationsMax);\n\tfor (int32 Iteration = 0; Iteration < PressureIterations; ++Iteration)\n\t{\n\t\tconst int32 NextPressureIndex = 1 - CurrentPressureIndex;\n\t\tAddPressureJacobiPass(\n\t\t\tGraphBuilder,\n\t\t\tState,\n\t\t\tState.AllocatedGridSize,\n\t\t\tPressureCellSize,\n\t\t\tPressureTextures[CurrentPressureIndex],\n\t\t\tDivergenceTexture,\n\t\t\tPressureTextures[NextPressureIndex],\n\t\t\tGetActiveBrickResourcesForPass(),\n\t\t\tIteration);\n\t\tCurrentPressureIndex = NextPressureIndex;\n\t}\n\tFRDGTextureRef PressureForProjection = PressureTextures[CurrentPressureIndex];\n"""
simulate = simulate[:solver_start] + jacobi_block + simulate[solver_end:]

finalize_start = simulate.find("\tif (bFinalizeFluidStep)")
finalize_end = simulate.find("\tif (IsPastSparseRenderableLifetime", finalize_start)
if finalize_start < 0 or finalize_end < 0:
    raise RuntimeError("Missing adaptive fluid finalization block")
simulate = simulate[:finalize_start] + simulate[finalize_end:]

old_splat_call = "\t\t\tVelocityTextures[VortexSplatWriteVelocityIndex],\n\t\t\tVortexDeltaSeconds);"
new_splat_call = "\t\t\tVelocityTextures[VortexSplatWriteVelocityIndex]);"
if simulate.count(old_splat_call) != 1:
    raise RuntimeError("Unexpected vortex splat call")
simulate = simulate.replace(old_splat_call, new_splat_call)
cpp = replace_function(cpp, "FTimeThiefSmokeViewExtension::SimulateSmoke", simulate)

pre_render = extract_function(cpp, "FTimeThiefSmokeViewExtension::PreRenderViewFamily_RenderThread")
fixed_start = pre_render.find("\t\t\t\tconst TArray<FTimeThiefSmokeRendererEvent> OriginalEvents")
fixed_end = pre_render.find("\n\n\t\t\t\tState.AccumulatedSimulationDeltaSeconds", fixed_start)
if fixed_start < 0 or fixed_end < 0:
    raise RuntimeError("Missing fixed-rate adaptive substep block")
fixed_replacement = """\t\t\t\tSimulateSmoke(\n\t\t\t\t\tGraphBuilder,\n\t\t\t\t\tState,\n\t\t\t\t\tSimulationInterval,\n\t\t\t\t\tSimulationInterval,\n\t\t\t\t\t!bHasFollowingSubstep);\n\t\t\t\tState.SimulationTimeSeconds += SimulationInterval;"""
pre_render = pre_render[:fixed_start] + fixed_replacement + pre_render[fixed_end:]

variable_start = pre_render.find("\t\t\tconst FVector3f CellSize", pre_render.find("\t\telse\n\t\t{"))
variable_end = pre_render.find("\n\n\t\t\tState.RenderTimeSeconds", variable_start)
if variable_start < 0 or variable_end < 0:
    raise RuntimeError("Missing variable-rate adaptive substep block")
variable_replacement = """\t\t\tSimulateSmoke(GraphBuilder, State, FrameDeltaSeconds, FrameDeltaSeconds, true);\n\t\t\tState.SimulationTimeSeconds += FrameDeltaSeconds;"""
pre_render = pre_render[:variable_start] + variable_replacement + pre_render[variable_end:]
cpp = replace_function(cpp, "FTimeThiefSmokeViewExtension::PreRenderViewFamily_RenderThread", pre_render)

splat = extract_function(cpp, "FTimeThiefSmokeViewExtension::AddVortexParticleSplatPass")
old_splat_signature = "\tFRDGTextureRef VelocityOut,\n\tfloat DeltaSeconds)"
new_splat_signature = "\tFRDGTextureRef VelocityOut)"
if splat.count(old_splat_signature) != 1:
    raise RuntimeError("Unexpected vortex splat definition signature")
splat = splat.replace(old_splat_signature, new_splat_signature)
if splat.count("\tPassParameters->DeltaSeconds = DeltaSeconds;\n") != 1:
    raise RuntimeError("Unexpected vortex splat DeltaSeconds assignment")
splat = splat.replace("\tPassParameters->DeltaSeconds = DeltaSeconds;\n", "")
cpp = replace_function(cpp, "FTimeThiefSmokeViewExtension::AddVortexParticleSplatPass", splat)
write(CPP, cpp)


# Remove shader sources that no longer have registrations or runtime callers.
for shader_name in ("TimeThiefSmokePressureMGPCG.usf", "TimeThiefSmokeVelocityMax.usf"):
    shader_path = SHADER_DIR / shader_name
    if not shader_path.exists():
        raise RuntimeError(f"Missing shader scheduled for deletion: {shader_name}")
    shader_path.unlink()


# Static validation.
forbidden = re.compile(
    r"MGPCG|PressureSolver|AdaptiveFluid|VelocityMaximum|VelocityMax|"
    r"QualityCourantLimit|MaxAdaptiveFluidSubsteps|VelocityReadbackIntervalFrames|"
    r"LastMeasuredMaxVelocity|LastVelocityMeasurementFrame|bVelocityMaximumReadbackPending|"
    r"AddMGPCGPressureSolvePasses"
)
scan_roots = [
    ROOT / "Plugins/TimeThiefSmokeRenderer",
    ROOT / "Plugins/TimeThiefSmokeTest",
]
violations: list[str] = []
for scan_root in scan_roots:
    for path in scan_root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in {".h", ".cpp", ".ush", ".usf"}:
            continue
        for line_number, line in enumerate(read(path).splitlines(), 1):
            if forbidden.search(line):
                violations.append(f"{path.relative_to(ROOT)}:{line_number}: {line}")
if violations:
    raise RuntimeError("Forbidden solver/adaptive references remain:\n" + "\n".join(violations))

expected_bullet_values = {
    "BulletClearRadius": "32.0f",
    "BulletClearRadiusRandomMin": "0.95f",
    "BulletWakeStrengthRandomMax": "1.0f",
    "BulletWakeMaxVisibleLife": "0.2f",
    "BulletWakeCutoutFeather": "1.5f",
    "BulletWakeMinLifeSeconds": "0.05f",
}
final_defaults = read(DEFAULTS_H)
for name, value in expected_bullet_values.items():
    if not re.search(rf"\b{name}\s*=\s*{re.escape(value)};", final_defaults):
        raise RuntimeError(f"Bullet wake default was not restored: {name}={value}")

final_cpp = read(CPP)
if "FTimeThiefSmokePressureJacobiCS" not in read(SHADERS_CPP):
    raise RuntimeError("Jacobi shader registration was removed")
if "AddPressureJacobiPass(" not in final_cpp:
    raise RuntimeError("Jacobi runtime path is missing")
if "AddPressureResidualPass(" not in final_cpp or "AddProjectionDiagnosticsPass(" not in final_cpp:
    raise RuntimeError("Projection diagnostics were unintentionally removed")
if (SHADER_DIR / "TimeThiefSmokePressureResidual.usf").exists() is False:
    raise RuntimeError("Pressure residual diagnostics shader was unintentionally removed")

subprocess.run(["git", "diff", "--check"], cwd=ROOT, check=True)
print("Smoke solver cleanup validation passed.")
