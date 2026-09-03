extends Node3D

@onready var sim: Node = $DinoIsland
@onready var ground: MeshInstance3D = $Ground
@onready var metrics_label: Label = $HUD/Metrics
@onready var aggression_slider: HSlider = $HUD/AggressionRow/AggressionSlider

var meshes: Array[MeshInstance3D] = []
var materials: Dictionary = {}
var paused: bool = false
var seed_value: int = 1
var aggression: float = 0.6

const SPECIES_RAPTOR := 0
const SPECIES_GALLI := 1
const SPECIES_TRIKE := 2
const SPECIES_PLAYER := 20

func _ready() -> void:
	_ensure_materials()
	aggression_slider.value_changed.connect(_on_aggression_changed)
	_reset_sim(false)
	_fit_ground()

func _reset_sim(naive: bool) -> void:
	sim.reset(seed_value, naive)
	sim.spawn_trike()
	sim.spawn_player_archetype(0)
	sim.set_raptor_knobs(aggression, 0.5, 0.5, 0.6, 0.5)
	sim.enable_lod(true)
	sim.set_paused(paused)

func _fit_ground() -> void:
	var extent: float = sim.arena_half_extent() * 2.0
	var plane := PlaneMesh.new()
	plane.size = Vector2(extent, extent)
	ground.mesh = plane
	if ground.get_surface_override_material(0) == null:
		var mat := StandardMaterial3D.new()
		mat.albedo_color = Color(0.32, 0.4, 0.26)
		mat.roughness = 0.95
		ground.set_surface_override_material(0, mat)

func _ensure_materials() -> void:
	materials[SPECIES_RAPTOR] = _make_mat(Color(0.85, 0.16, 0.12))
	materials[SPECIES_GALLI] = _make_mat(Color(0.22, 0.76, 0.28))
	materials[SPECIES_TRIKE] = _make_mat(Color(0.22, 0.42, 0.92))
	materials[SPECIES_PLAYER] = _make_mat(Color(0.95, 0.86, 0.16))
	materials[-1] = _make_mat(Color(0.55, 0.55, 0.55))

func _make_mat(color: Color) -> StandardMaterial3D:
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mat.roughness = 0.7
	return mat

func _species_material(species: int) -> StandardMaterial3D:
	if materials.has(species):
		return materials[species]
	return materials[-1]

func _on_aggression_changed(value: float) -> void:
	aggression = value
	sim.set_raptor_knobs(aggression, 0.5, 0.5, 0.6, 0.5)

func _unhandled_input(event: InputEvent) -> void:
	if not (event is InputEventKey) or not event.pressed or event.echo:
		return
	match event.keycode:
		KEY_N:
			sim.set_naive(not sim.is_naive())
		KEY_P:
			paused = not paused
			sim.set_paused(paused)
		KEY_R:
			_reset_sim(sim.is_naive())
		KEY_1:
			sim.spawn_player_archetype(0)
		KEY_2:
			sim.spawn_player_archetype(1)
		KEY_3:
			sim.spawn_player_archetype(2)
		KEY_4:
			sim.spawn_player_archetype(3)
		KEY_5:
			sim.spawn_player_archetype(4)

func _process(_dt: float) -> void:
	var n: int = sim.agent_count()
	while meshes.size() < n:
		var mi := MeshInstance3D.new()
		var sphere := SphereMesh.new()
		sphere.radius = 0.6
		mi.mesh = sphere
		add_child(mi)
		meshes.append(mi)
	for i in range(n):
		var p: Vector3 = sim.agent_position(i)
		var species: int = sim.agent_species(i)
		meshes[i].position = p + Vector3(0, 0.6, 0)
		meshes[i].material_override = _species_material(species)
		meshes[i].scale = Vector3.ONE * (1.45 if species == SPECIES_TRIKE else 1.0)
		meshes[i].visible = true
	for i in range(n, meshes.size()):
		meshes[i].visible = false
	_update_metrics(n)

func _update_metrics(n: int) -> void:
	var mode := "NAIVE" if sim.is_naive() else "SCRIPTED"
	var pause_txt := "PAUSED" if paused else "RUNNING"
	metrics_label.text = (
		"Feel A/B: %s   %s\n" % [mode, pause_txt]
		+ "Agents: %d\n" % n
		+ "Raptor hunger: %.0f%%\n" % (sim.mean_raptor_hunger() * 100.0)
		+ "Raptor kills: %d\n" % sim.raptor_kills()
		+ "Rear-arc fraction: %.3f\n" % sim.rear_arc_fraction()
		+ "Immigrant rate: %.3f\n" % sim.immigrant_rate()
		+ "LOD L0 (full policy): %d\n" % sim.lod_policy_count()
		+ "GRU resets last tick: %d\n" % sim.gru_resets_last_tick()
		+ "Aggression: %.2f" % aggression
	)
