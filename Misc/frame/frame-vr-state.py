"""Read-only SteamVR scene/controller snapshot. Run on Frame next to openvr_capi.h.

Uses the exact FnTable version and order from Valve's saved header. Initializes
as a utility (3), never as a scene application, and shuts down only its client.
OpenVR device state is runtime evidence; per-game OpenXR actions need game logs.
"""
import ctypes as C
import json
import re
from pathlib import Path
from datetime import datetime, timezone

header = Path(__file__).with_name('openvr_capi.h').read_text()
vr = C.CDLL('/opt/steamvr/bin/linuxarm64/libopenvr_api.so')
vr.VR_InitInternal2.argtypes = [C.POINTER(C.c_int), C.c_int, C.c_char_p]
vr.VR_InitInternal2.restype = C.c_uint32
vr.VR_GetGenericInterface.argtypes = [C.c_char_p, C.POINTER(C.c_int)]
vr.VR_GetGenericInterface.restype = C.c_void_p

def table(interface):
    version = re.search(r'\b' + interface + r'_Version\s*=\s*"([^"]+)"', header)[1]
    body = re.search(r'struct VR_' + interface + r'_FnTable\s*\{(.*?)\};', header, re.S)[1]
    names = re.findall(r'\*([A-Za-z0-9_]+)\)', body)
    err = C.c_int()
    ptr = vr.VR_GetGenericInterface(('FnTable:' + version).encode(), C.byref(err))
    if not ptr or err.value:
        raise RuntimeError(f'{version}: interface error {err.value}; use matching Valve header')
    pointers = C.cast(ptr, C.POINTER(C.c_void_p))
    def function(name, result, *args):
        return C.CFUNCTYPE(result, *args)(pointers[names.index(name)])
    return function, version

class Pose(C.Structure):
    _fields_ = [('matrix', (C.c_float * 4) * 3), ('velocity', C.c_float * 3),
                ('angular_velocity', C.c_float * 3), ('result', C.c_int),
                ('valid', C.c_bool), ('connected', C.c_bool)]

class Axis(C.Structure):
    _fields_ = [('x', C.c_float), ('y', C.c_float)]

class ControllerState(C.Structure):
    _fields_ = [('packet', C.c_uint32), ('pressed', C.c_uint64),
                ('touched', C.c_uint64), ('axes', Axis * 5)]

err = C.c_int()
vr.VR_InitInternal2(C.byref(err), 3, None)
if err.value:
    raise RuntimeError(f'SteamVR utility initialization: {err.value}')
try:
    apps, av = table('IVRApplications')
    system, sv = table('IVRSystem')
    comp, cv = table('IVRCompositor')
    overlay, ov = table('IVROverlay')
    chaperone, chv = table('IVRChaperone')
    scene = apps('GetCurrentSceneProcessId', C.c_uint32)()
    key = C.create_string_buffer(1024)
    key_error = apps('GetApplicationKeyByProcessId', C.c_int, C.c_uint32,
                     C.c_char_p, C.c_uint32)(scene, key, len(key))
    state = apps('GetSceneApplicationState', C.c_int)()
    state_name = apps('GetSceneApplicationStateNameFromEnum', C.c_char_p, C.c_int)(state)
    poses = (Pose * 64)()
    system('GetDeviceToAbsoluteTrackingPose', None, C.c_int, C.c_float,
           C.POINTER(Pose), C.c_uint32)(1, 0, poses, 64)
    devices = []
    for i, pose in enumerate(poses):
        cls = system('GetTrackedDeviceClass', C.c_int, C.c_uint32)(i)
        if cls not in (1, 2):
            continue
        d = {'index': i, 'class': {1: 'HMD', 2: 'Controller'}[cls],
             'connected': pose.connected, 'pose_valid': pose.valid,
             'tracking_result': pose.result,
             'position': [round(pose.matrix[j][3], 4) for j in range(3)],
             'activity': system('GetTrackedDeviceActivityLevel', C.c_int, C.c_uint32)(i)}
        for label, prop in [('model', 1001), ('controller_type', 7000)]:
            text = C.create_string_buffer(256)
            prop_error = C.c_int()
            system('GetStringTrackedDeviceProperty', C.c_uint32, C.c_uint32, C.c_int,
                   C.c_char_p, C.c_uint32, C.POINTER(C.c_int))(
                       i, prop, text, len(text), C.byref(prop_error))
            if prop_error.value == 0:
                d[label] = text.value.decode()
        if cls == 2:
            d['role'] = system('GetControllerRoleForTrackedDeviceIndex', C.c_int, C.c_uint32)(i)
            cs = ControllerState()
            d['legacy_state_available'] = system('GetControllerState', C.c_bool, C.c_uint32,
                C.POINTER(ControllerState), C.c_uint32)(i, C.byref(cs), C.sizeof(cs))
            d['legacy_buttons_pressed'] = hex(cs.pressed)
        devices.append(d)
    pidpath = Path(f'/proc/{scene}')
    area_x, area_z = C.c_float(), C.c_float()
    area_available = chaperone('GetPlayAreaSize', C.c_bool,
        C.POINTER(C.c_float), C.POINTER(C.c_float))(C.byref(area_x), C.byref(area_z))
    out = {'utc': datetime.now(timezone.utc).isoformat(), 'interfaces': [av, sv, cv, ov, chv],
           'scene_pid': scene, 'scene_key': key.value.decode(), 'key_error': key_error,
           'scene_state': state_name.decode() if state_name else str(state),
           'compositor_scene_focus_pid': comp('GetCurrentSceneFocusProcess', C.c_uint32)(),
           'dashboard_visible': overlay('IsDashboardVisible', C.c_bool)(),
           'chaperone': {'calibration_state': chaperone('GetCalibrationState', C.c_int)(),
                         'bounds_visible': chaperone('AreBoundsVisible', C.c_bool)(),
                         'play_area_available': area_available,
                         'play_area_metres': [area_x.value, area_z.value]},
           'devices': devices,
           'note': 'Device poses alone do not prove the game receives OpenXR actions or immersive output.'}
    if scene and pidpath.exists():
        out['scene_exe'] = str((pidpath/'exe').resolve())
        out['scene_arguments'] = (pidpath/'cmdline').read_bytes().replace(b'\0', b' ').decode()
    print(json.dumps(out, indent=2))
finally:
    vr.VR_ShutdownInternal()
