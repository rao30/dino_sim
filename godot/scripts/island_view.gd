extends Node3D

## Feel harness view: syncs Quaternius skeletal dinos to C++ island poses.
## No gameplay / AI here — only presentation.

@onready var sim: Node = $DinoIsland
@onready var ground: MeshInstance3D = $Ground
@onready var agents_root: Node3D = $Agents
@onready var camera: Camera3D = $Camera3D
@onready var metrics_label: Label = $HUD/Metrics
@onready var aggression_slider: HSlider = $HUD/AggressionRow/AggressionSlider

var agent_nodes: Array[Node3D] = []
var scene_cache: Dictionary = {}  # path -> PackedScene
var paused: bool = false
var seed_value: int = 1
var aggression: float = 0.6
var follow_camera: bool = true

const LOCO_IDLE := 0
const LOCO_WALK := 1
const LOCO_SPRINT := 2

# Sim heading 0 faces +X (cos/sin on XZ). Quaternius GLB faces -Z after Y-up export.
const YAW_OFFSET := -PI * 0.5

# Species id -> glb path under res://assets/dinos/
const SPECIES_MODEL := {
	0: "res://assets/dinos/Velociraptor.glb",   # Utahraptor
	1: "res://assets/dinos/Parasaurolophus.glb", # Gallimimus (closest biped herbivore)
	2: "res://assets/dinos/Triceratops.glb",
	5: "res://assets/dinos/Parasaurolophus.glb", # Parasaur
	6: "res://assets/dinos/Stegosaurus.glb",
	9: "res://assets/dinos/Apatosaurus.glb",     # Brontosaurus
	18: "res://assets/dinos/Trex.glb",           # Tyrannosaurus
	19: "res://assets/dinos/Trex.glb",           # Spinosaurus stand-in
	20: "res://assets/dinos/Trex.glb",           # Player
}

# Tuned so model footprint roughly matches sim body_radius.
const SPECIES_SCALE := {
	0: 1.25,
	1: 1.05,
	2: 1.45,
	5: 1.15,
	6: 1.35,
	9: 1.85,
	18: 1.7,
	19: 1.8,
	20: 1.25,
}

# Nominal max_speed (from types) used to drive AnimationPlayer.speed_scale.
const SPECIES_REF_SPEED := {
	0: 16.0,
	1: 20.0,
	2: 8.0,
	5: 11.0,
	6: 7.0,
	9: 5.0,
	18: 11.0,
	19: 10.0,
	20: 7.5,
}

func _ready() -> void:
	aggression_slider.value_changed.connect(_on_aggression_changed)
	_reset_sim(false)
	_fit_ground()
	# Start with a readable high orbit over the chase arena.
	camera.global_position = Vector3(0, 52, 58)
	camera.look_at(Vector3(0, 1.5, 0), Vector3.UP)

func _reset_sim(naive: bool) -> void:
	sim.reset(seed_value, naive)
	sim.spawn_trike()
	sim.spawn_player_archetype(0)
	sim.set_raptor_knobs(aggression, 0.5, 0.5, 0.6, 0.5)
	sim.enable_lod(true)
	sim.set_paused(paused)
	_clear_agents()

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

func _clear_agents() -> void:
	for n in agent_nodes:
		if is_instance_valid(n):
			n.queue_free()
	agent_nodes.clear()

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
		KEY_C:
			follow_camera = not follow_camera
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
	while agent_nodes.size() < n:
		agent_nodes.append(_make_placeholder())
	var alive_n := 0
	var loco_counts := [0, 0, 0]
	for i in range(n):
		_sync_agent(i)
		if sim.agent_alive(i):
			alive_n += 1
			var ls: int = clampi(sim.agent_loco_state(i), 0, 2)
			loco_counts[ls] += 1
	for i in range(n, agent_nodes.size()):
		agent_nodes[i].visible = false
	_update_camera(n)
	_update_metrics(alive_n, n, loco_counts)

func _make_placeholder() -> Node3D:
	var root := Node3D.new()
	agents_root.add_child(root)
	root.set_meta("species", -1)
	root.set_meta("loco", -1)
	return root

func _packed_scene_for(path: String) -> PackedScene:
	if scene_cache.has(path):
		return scene_cache[path]
	var ps: PackedScene = load(path)
	scene_cache[path] = ps
	return ps

func _strip_model(root: Node3D) -> void:
	for c in root.get_children():
		c.queue_free()
	root.set_meta("species", -1)
	root.set_meta("loco", -1)

func _ensure_model(root: Node3D, species: int) -> void:
	if int(root.get_meta("species")) == species and root.get_child_count() > 0:
		return
	_strip_model(root)
	root.set_meta("species", species)
	root.set_meta("loco", -1)
	var path: Variant = SPECIES_MODEL.get(species, null)
	if path == null:
		var mi := MeshInstance3D.new()
		var sphere := SphereMesh.new()
		sphere.radius = 0.6
		mi.mesh = sphere
		var mat := StandardMaterial3D.new()
		mat.albedo_color = Color(0.55, 0.55, 0.55)
		mi.material_override = mat
		root.add_child(mi)
		return
	var inst: Node3D = _packed_scene_for(path).instantiate()
	inst.name = "Model"
	root.add_child(inst)
	var s: float = float(SPECIES_SCALE.get(species, 1.2))
	inst.scale = Vector3.ONE * s

func _find_anim_player(node: Node) -> AnimationPlayer:
	if node is AnimationPlayer:
		return node
	for c in node.get_children():
		var found := _find_anim_player(c)
		if found:
			return found
	return null

func _pick_anim(player: AnimationPlayer, kind: String) -> String:
	var names := player.get_animation_list()
	var best := ""
	for n in names:
		var low := String(n).to_lower()
		if kind in low:
			best = n
			if "loop" in low:
				return n
	return best

func _apply_loco(root: Node3D, loco: int, speed: float, species: int) -> void:
	var model: Node = root.get_node_or_null("Model")
	if model == null:
		return
	var player := _find_anim_player(model)
	if player == null:
		return
	var kind := "idle"
	if loco == LOCO_WALK:
		kind = "walk"
	elif loco == LOCO_SPRINT:
		kind = "run"
	var clip := _pick_anim(player, kind)
	if clip == "":
		return
	if int(root.get_meta("loco")) != loco or player.current_animation != clip:
		root.set_meta("loco", loco)
		player.play(clip)
	# Match foot cadence to sim speed to cut foot-slide.
	var ref: float = float(SPECIES_REF_SPEED.get(species, 12.0))
	var target := 1.0
	if loco == LOCO_WALK:
		target = clampf(speed / maxf(ref * 0.45, 0.1), 0.55, 1.35)
	elif loco == LOCO_SPRINT:
		target = clampf(speed / maxf(ref * 0.85, 0.1), 0.7, 1.55)
	else:
		target = 1.0
	player.speed_scale = target

func _sync_agent(i: int) -> void:
	var root: Node3D = agent_nodes[i]
	var alive: bool = sim.agent_alive(i)
	root.visible = alive
	if not alive:
		# Drop skeletal mesh so dead immigrant slots don't keep AnimationPlayers alive.
		if root.get_child_count() > 0:
			_strip_model(root)
		return
	var species: int = sim.agent_species(i)
	_ensure_model(root, species)
	var p: Vector3 = sim.agent_position(i)
	root.position = p
	var heading: float = sim.agent_heading(i)
	root.rotation = Vector3(0.0, -(heading) + YAW_OFFSET, 0.0)
	var loco: int = sim.agent_loco_state(i)
	var speed: float = sim.agent_speed(i)
	_apply_loco(root, loco, speed, species)

func _update_camera(n: int) -> void:
	if not follow_camera or n <= 0:
		return
	# Prefer live raptors / players so the chase stays framed as immigrants pile up.
	var sum := Vector3.ZERO
	var count := 0
	for i in range(n):
		if not sim.agent_alive(i):
			continue
		var sp: int = sim.agent_species(i)
		if sp != 0 and sp != 20 and sp != 2:
			continue
		sum += sim.agent_position(i)
		count += 1
	if count == 0:
		for i in range(n):
			if not sim.agent_alive(i):
				continue
			sum += sim.agent_position(i)
			count += 1
	if count == 0:
		return
	var center := sum / float(count)
	var target := center + Vector3(0, 46, 54)
	camera.global_position = camera.global_position.lerp(target, 0.06)
	camera.look_at(center + Vector3(0, 1.2, 0), Vector3.UP)

func _update_metrics(alive_n: int, slots: int, loco_counts: Array) -> void:
	var mode := "NAIVE" if sim.is_naive() else "SCRIPTED"
	var pause_txt := "PAUSED" if paused else "RUNNING"
	metrics_label.text = (
		"Feel A/B: %s   %s\n" % [mode, pause_txt]
		+ "Alive: %d   slots: %d   Quaternius GLB\n" % [alive_n, slots]
		+ "Loco idle/walk/run: %d / %d / %d\n" % [loco_counts[0], loco_counts[1], loco_counts[2]]
		+ "Raptor hunger: %.0f%%\n" % (sim.mean_raptor_hunger() * 100.0)
		+ "Raptor kills: %d\n" % sim.raptor_kills()
		+ "Rear-arc fraction: %.3f\n" % sim.rear_arc_fraction()
		+ "Immigrant rate: %.3f\n" % sim.immigrant_rate()
		+ "LOD L0 (full policy): %d\n" % sim.lod_policy_count()
		+ "Aggression: %.2f\n" % aggression
		+ "C camera follow   N naive   P pause   R reset"
	)
