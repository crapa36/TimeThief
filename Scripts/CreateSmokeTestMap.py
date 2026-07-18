import unreal


MAP_PATH = "/Game/SmokeTest/SmokeTestMap"


level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
unreal_editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

if not level_editor.new_level(MAP_PATH):
    raise RuntimeError(f"Failed to create {MAP_PATH}")

camera = actor_editor.spawn_actor_from_class(
    unreal.CameraActor,
    unreal.Vector(-1800.0, 0.0, 350.0),
    unreal.Rotator(0.0, -8.0, 0.0),
)
camera.set_actor_label("SmokeTestCamera")

light = actor_editor.spawn_actor_from_class(
    unreal.DirectionalLight,
    unreal.Vector(0.0, 0.0, 500.0),
    unreal.Rotator(-35.0, -45.0, 0.0),
)
light.set_actor_label("SmokeTestDirectionalLight")
light.light_component.set_intensity(4.0)

floor = actor_editor.spawn_actor_from_class(
    unreal.StaticMeshActor,
    unreal.Vector(0.0, 0.0, 0.0),
    unreal.Rotator(),
)
floor.set_actor_label("SmokeTestFloor")
floor.static_mesh_component.set_static_mesh(
    unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
)
floor.set_actor_scale3d(unreal.Vector(20.0, 20.0, 20.0))

world = unreal_editor.get_editor_world()
world.world_settings.set_editor_property("default_game_mode", None)

if not level_editor.save_current_level():
    raise RuntimeError(f"Failed to save {MAP_PATH}")

unreal.log(f"Created {MAP_PATH}")
