#pragma once

#include <cstdint>
#include "GeoCommand.h"

#ifdef __cplusplus
extern "C" {
#endif

int16_t* read_vec3s_to_vec3f(Vec3f& dst, int16_t *src);
int16_t* read_vec3s(Vec3s& dst, int16_t *src);

#ifdef __cplusplus
}
#endif