#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include "opus.h"

using namespace godot;

namespace godot {

class Opus : public Object
{
	GDCLASS(Opus, Object);
	static Opus *singleton;

protected:
	static void _bind_methods();

public:
	static Opus *get_singleton();

	Opus();
	~Opus();

	PackedFloat32Array encode(PackedVector2Array input);
	PackedVector2Array decode(PackedFloat32Array input);
private:
	OpusEncoder* m_encoder;
	OpusDecoder* m_decoder;

};

}
