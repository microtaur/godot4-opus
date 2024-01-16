# godot4-opus

A GDExtension that adds to Godot4 support for encoding voice data using the Opus Codec. This can be useful for those who want to implement VOIP systems for Godot4 games.

## Overview

This extension adds a new singleton to Godot: `Opus` with two methods: `encode` and `decode`.

These can be used to compress audio obtained `AudioEffectCapture` and then to decode it so it's usable in Godot again.

Quick and dirty example (full demo coming soon):

```GDScript

func _process_audio() -> void:
    if has_data() and active:
        call_deferred("rpc", "play_data", get_data())

...

func get_data() -> PackedFloat32Array:
    var data = effect.get_buffer(BUFFER_SIZE)
    return Opus.encode(data)

...

@rpc("any_peer", "call_remote", "unreliable_ordered")
func play_data(data: PackedFloat32Array) -> void:
    var id = client.multiplayer.get_remote_sender_id()
    var decoded = Opus.decode(data)
    for b in range(0, BUFFER_SIZE):
        _get_generator(id).push_frame(decoded[b])

```

## Known limitations

- This is more POC than a production-ready solution although it's not too far from achieving this status. I'm using this code successfully in a scenario similar to the one presented in this demo:
https://github.com/godotengine/godot-demo-projects/tree/master/networking/webrtc_signaling
- Buffer size must be == 480
- Quality settings are currently hardcoded. Bitrate is 24000; bandwith is OPUS_BANDWIDTH_SUPERWIDEBAND.
- At the moment this works only on Windows x64. I haven't tried other platforms. Pre-built version of libopus resides in `3rdparty` directory.

## License

This is MIT licensed. Project uses Opus Codec which is BSD-licensed.
