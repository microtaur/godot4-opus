# godot4-opus

A GDExtension that adds to Godot4 support for encoding voice data using the Opus Codec. This can be useful for those who want to implement VOIP systems for Godot4 games.

## Overview

This extension adds a new node to Godot: `Opus` with three methods: `encode`, `decode` and `decode_and_play`.

These can be used to compress audio obtained `AudioEffectCapture` and then to decode it so it's usable in Godot again.

Usage is best illustrated in this demo: https://github.com/microtaur/godot4-p2p-voip

Most interesting part from the demo code illustrating how this works:

```GDScript

var _encoder := Opus.new()

...

func _audio_process():
	while true:
		if has_data() and active:
			var data = get_data()
			call_deferred("rpc", "play_data", data)
		else:
			OS.delay_msec(10)

...

func get_data() -> PackedFloat32Array:
	var data = effect.get_buffer(BUFFER_SIZE)
	return _encoder.encode(data)

...

@rpc("any_peer", "call_remote", "reliable")
func play_data(data: PackedFloat32Array) -> void:
	var id = client.multiplayer.get_remote_sender_id()
	_update_player_pool()
	_get_opus_instance(id).decode_and_play(_get_generator(id), data)

```

## Building
In order to build this project, you need scons. Currently only Windows x64 (MSVC) is supported.

1. Clone repo with submodules (godot-cpp)
```
git clone --recurse-submodules https://github.com/microtaur/godot4-opus.git && cd godot4-opus
```

2. Build debug
```
scons
```

3. Build release
```
scons target=template_release
```

More information on building GDExtensions: [https://docs.godotengine.org/pl/4.x/tutorials/scripting/gdextension/gdextension_cpp_example.html](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/gdextension_cpp_example.html)

## Known limitations

- This is more POC than a production-ready solution although it's not too far from achieving this status. I'm using this code successfully in a scenario similar to the one presented in this demo:
https://github.com/godotengine/godot-demo-projects/tree/master/networking/webrtc_signaling
- Buffer size must be == 480
- Quality settings are currently hardcoded. Bitrate is 24000; bandwith is OPUS_BANDWIDTH_SUPERWIDEBAND.
- At the moment this works only on Windows x64. I haven't tried other platforms. Pre-built version of libopus resides in `3rdparty` directory.

## License

This is MIT licensed. Project uses Opus Codec which is BSD-licensed.
