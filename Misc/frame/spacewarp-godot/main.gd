extends Node3D
## Deterministic moving-content scene for comparing frame synthesis (application motion vectors) across engines.
## Godot supplies the motion vectors and depth; this script only moves objects, loads the GPU and writes timing logs.
##
## User arguments (after `--`):
##   --fs on|off|project   frame synthesis at runtime (OpenXRFrameSynthesisExtension.enabled); `project` leaves the project setting alone
##   --relax               request the relaxed frame interval flag
##   --load-iters K        fragment-shader loop iterations on a head-locked full-view quad (GPU load)
##   --seconds S           run length counted from the first FOCUSED (default 18)
##   --warmup-seconds W    skipped at the start of the measured window (default 6)
##   --focus-timeout S     give up (exit 4) if never FOCUSED (default 40)
##   --summary FILE / --log FILE
## Exit codes: 0 ok, 3 frame synthesis extension missing/unavailable when requested, 4 runtime failure.

const LOAD_SHADER := """
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled;
uniform int iters = 0;
void fragment() {
	float acc = FRAGCOORD.x * 0.001 + FRAGCOORD.y * 0.002;
	for (int i = 0; i < iters; i++) {
		acc = sin(acc * 1.7 + float(i)) * cos(acc * 0.9 - float(i) * 0.5) + acc * 0.5;
	}
	ALBEDO = vec3(abs(acc) * 1e-6);
	ALPHA = 1.0;
}
"""

const FLOOR_SHADER := """
shader_type spatial;
void fragment() {
	vec2 p = floor(UV * 40.0);
	float c = mod(p.x + p.y, 2.0);
	ALBEDO = mix(vec3(0.15), vec3(0.65), c);
}
"""

var fs_mode := "on"
var relax := false
var load_iters := 0
var run_seconds := 18.0
var warmup_seconds := 6.0
var focus_timeout := 40.0
var summary_path := ""
var log_path := ""

var xr: OpenXRInterface
var fs_ext: Object
var log_file: FileAccess
var vp_rid: RID

var started_us := 0
var focused := false
var focused_us := 0
var states: Array[String] = []
var sim_t := 0.0
var last_us := 0
var frames_total := 0
var window_frames := 0
var unfocused_in_window := 0
var frame_ms: Array[float] = []
var gpu_ms: Array[float] = []
var cpu_ms: Array[float] = []
var sec_frame: Array[float] = []
var sec_gpu: Array[float] = []
var sec_index := 0
var finished := false

var mover: MeshInstance3D
var spinner: MeshInstance3D
var orbiter: MeshInstance3D


func _ready() -> void:
	_parse_args()
	log_file = FileAccess.open(log_path, FileAccess.WRITE) if log_path != "" else null
	started_us = Time.get_ticks_usec()
	_log("INFO", "godot=%s adapter=%s load_iters=%d fs=%s relax=%s" % [
		Engine.get_version_info().string, RenderingServer.get_video_adapter_name(), load_iters, fs_mode, str(relax)])
	xr = XRServer.find_interface("OpenXR") as OpenXRInterface
	if xr == null or not xr.is_initialized():
		_log("ERROR", "OpenXR interface missing or not initialized (run with --xr-mode on)")
		_finish("openxr_unavailable", 4)
		return
	vp_rid = get_viewport().get_viewport_rid()
	get_viewport().use_xr = true
	RenderingServer.viewport_set_measure_render_time(vp_rid, true)
	xr.session_visible.connect(func(): _state("VISIBLE"))
	xr.session_focussed.connect(_on_focussed)
	xr.session_stopping.connect(func(): _state("STOPPING"))
	xr.session_synchronized.connect(func(): _state("SYNCHRONIZED"))
	_log("INFO", "xr render target=%s display_refresh=%.1f" % [str(xr.get_render_target_size()), xr.get_display_refresh_rate()])
	_setup_frame_synthesis()
	_build_world()


func _parse_args() -> void:
	var a := OS.get_cmdline_user_args()
	var i := 0
	while i < a.size():
		var k := a[i]
		var v := a[i + 1] if i + 1 < a.size() else ""
		match k:
			"--fs": fs_mode = v; i += 1
			"--relax": relax = true
			"--load-iters": load_iters = int(v); i += 1
			"--seconds": run_seconds = float(v); i += 1
			"--warmup-seconds": warmup_seconds = float(v); i += 1
			"--focus-timeout": focus_timeout = float(v); i += 1
			"--summary": summary_path = v; i += 1
			"--log": log_path = v; i += 1
		i += 1


func _setup_frame_synthesis() -> void:
	fs_ext = Engine.get_singleton("OpenXRFrameSynthesisExtension") if Engine.has_singleton("OpenXRFrameSynthesisExtension") else null
	if fs_ext == null:
		_log("WARN", "OpenXRFrameSynthesisExtension singleton not found")
		if fs_mode == "on":
			_log("ERROR", "frame synthesis requested but not available")
			_finish("synthesis_missing", 3)
		return
	_log("INFO", "frame synthesis available=%s enabled(before)=%s" % [str(fs_ext.is_available()), str(fs_ext.enabled)])
	if fs_mode != "project":
		fs_ext.enabled = (fs_mode == "on")
	fs_ext.relax_frame_interval = relax
	_log("INFO", "frame synthesis enabled(now)=%s relax=%s" % [str(fs_ext.enabled), str(fs_ext.relax_frame_interval)])


func _on_focussed() -> void:
	_state("FOCUSED")
	if focused_us == 0:
		focused_us = Time.get_ticks_usec()
	focused = true


func _state(state_name: String) -> void:
	if state_name != "FOCUSED" and focused:
		focused = false
	states.append(state_name)
	_log("STATE", state_name)


func _build_world() -> void:
	var env_node := WorldEnvironment.new()
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.35, 0.5, 0.8)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.7, 0.7, 0.7)
	env_node.environment = env
	add_child(env_node)

	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-50, 30, 0)
	add_child(sun)

	var floor_mesh := MeshInstance3D.new()
	var plane := PlaneMesh.new()
	plane.size = Vector2(20, 20)
	floor_mesh.mesh = plane
	var floor_mat := ShaderMaterial.new()
	floor_mat.shader = Shader.new()
	floor_mat.shader.code = FLOOR_SHADER
	floor_mesh.material_override = floor_mat
	add_child(floor_mesh)

	mover = _box(Vector3(0.3, 0.3, 0.3), Color(0.9, 0.2, 0.2), Vector3(0, 1.4, -1.35))
	spinner = _box(Vector3(0.35, 0.35, 0.35), Color(0.2, 0.8, 0.3), Vector3(-0.8, 1.2, -2.0))
	var sphere := MeshInstance3D.new()
	var sm := SphereMesh.new()
	sm.radius = 0.15
	sm.height = 0.3
	sphere.mesh = sm
	var sphere_mat := StandardMaterial3D.new()
	sphere_mat.albedo_color = Color(0.95, 0.85, 0.2)
	sphere.material_override = sphere_mat
	add_child(sphere)
	orbiter = sphere
	for z in [-3.0, -4.5, -6.0]:
		for x in [-1.5, 1.5]:
			var pillar := _box(Vector3(0.25, 2.0, 0.25), Color(0.6, 0.6, 0.7), Vector3(x, 1.0, z))
			pillar.name = "pillar_%d_%d" % [int(x * 10), int(z * 10)]

	var origin := XROrigin3D.new()
	add_child(origin)
	var cam := XRCamera3D.new()
	origin.add_child(cam)
	if load_iters > 0:
		var q := MeshInstance3D.new()
		var quad := QuadMesh.new()
		quad.size = Vector2(4, 4)
		q.mesh = quad
		q.position = Vector3(0, 0, -0.3)
		var m := ShaderMaterial.new()
		m.shader = Shader.new()
		m.shader.code = LOAD_SHADER
		m.set_shader_parameter("iters", load_iters)
		q.material_override = m
		q.extra_cull_margin = 16384.0
		cam.add_child(q)


func _box(size: Vector3, color: Color, pos: Vector3) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = size
	mi.mesh = bm
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mi.material_override = mat
	mi.position = pos
	add_child(mi)
	return mi


func _process(delta: float) -> void:
	if finished:
		return
	var now := Time.get_ticks_usec()
	frames_total += 1
	sim_t += delta
	if mover != null:
		mover.position.x = 0.5 * sin(sim_t * 1.2)
		spinner.rotation.y = sim_t * TAU * 0.25
		spinner.rotation.x = sim_t * TAU * 0.1
		orbiter.position = Vector3(0.8 + 0.4 * cos(sim_t * PI), 1.3, -2.2 + 0.4 * sin(sim_t * PI))
	if focused_us == 0:
		if (now - started_us) / 1.0e6 > focus_timeout:
			_log("ERROR", "never FOCUSED within %.0f s" % focus_timeout)
			_finish("focus_timeout", 4)
		last_us = now
		return
	var elapsed := (now - focused_us) / 1.0e6
	var dt_ms := (now - last_us) / 1000.0
	last_us = now
	var gpu := RenderingServer.viewport_get_measured_render_time_gpu(vp_rid)
	var cpu := RenderingServer.viewport_get_measured_render_time_cpu(vp_rid) + RenderingServer.get_frame_setup_time_cpu()
	if elapsed >= warmup_seconds and elapsed <= run_seconds:
		window_frames += 1
		if not focused:
			unfocused_in_window += 1
		frame_ms.append(dt_ms)
		gpu_ms.append(gpu)
		cpu_ms.append(cpu)
	sec_frame.append(dt_ms)
	sec_gpu.append(gpu)
	if int(elapsed) > sec_index:
		_flush_second(elapsed)
		sec_index = int(elapsed)
	if elapsed > run_seconds:
		_finish("completed", 0)


func _flush_second(elapsed: float) -> void:
	_log("SEC", "t=%.1f state=%s frames=%d frame_avg=%.2f frame_max=%.2f gpu_avg=%.2f refresh=%.1f" % [
		elapsed, states.back() if states.size() > 0 else "-", sec_frame.size(), _avg(sec_frame), _maxv(sec_frame),
		_avg(sec_gpu), xr.get_display_refresh_rate() if xr else 0.0])
	sec_frame.clear()
	sec_gpu.clear()


func _stats(values: Array[float]) -> Dictionary:
	if values.is_empty():
		return {"avg": 0.0, "p50": 0.0, "p95": 0.0, "p99": 0.0, "max": 0.0, "min": 0.0, "n": 0}
	var s = values.duplicate()
	s.sort()
	var n := s.size()
	return {"avg": _avg(values), "p50": s[int(0.5 * (n - 1))], "p95": s[int(0.95 * (n - 1))],
		"p99": s[int(0.99 * (n - 1))], "max": s[n - 1], "min": s[0], "n": n}


func _avg(values: Array[float]) -> float:
	if values.is_empty():
		return 0.0
	var t := 0.0
	for v in values:
		t += v
	return t / values.size()


func _maxv(values: Array[float]) -> float:
	var m := 0.0
	for v in values:
		m = maxf(m, v)
	return m


func _log(tag: String, msg: String) -> void:
	var line := "%s %s" % [tag, msg]
	print(line)
	if log_file != null:
		log_file.store_line(line)
		log_file.flush()


func _finish(reason: String, code: int) -> void:
	if finished:
		return
	finished = true
	var window_s := maxf(0.0, run_seconds - warmup_seconds)
	var summary := {
		"api": "godot-fs-" + fs_mode,
		"engine": Engine.get_version_info().string,
		"exit_reason": reason,
		"exit_code": code,
		"frames": window_frames,
		"seconds": window_s,
		"fps": (window_frames / window_s) if window_s > 0.0 else 0.0,
		"unfocused_frames": unfocused_in_window,
		"frame_ms": _stats(frame_ms),
		"cpu_busy_ms": _stats(cpu_ms),
		"gpu_ms": _stats(gpu_ms),
		"load_iters": load_iters,
		"fs_mode": fs_mode,
		"fs_singleton": fs_ext != null,
		"fs_available": fs_ext.is_available() if fs_ext != null else false,
		"fs_enabled": fs_ext.enabled if fs_ext != null else false,
		"relax": relax,
		"warmup_seconds": warmup_seconds,
		"run_seconds": run_seconds,
		"states": states,
		"display_refresh_hz": xr.get_display_refresh_rate() if xr != null else 0.0,
		"adapter": RenderingServer.get_video_adapter_name(),
	}
	_log("SUMMARY", JSON.stringify(summary))
	if summary_path != "":
		var f := FileAccess.open(summary_path, FileAccess.WRITE)
		if f != null:
			f.store_string(JSON.stringify(summary, "  "))
			f.close()
	get_tree().quit(code)
