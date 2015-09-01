#version 330

uniform sampler2D uSampler;

uniform vec2 texture_size;

in vec2 pos;

out vec4 color;

void main() {
  vec2 uv = pos * 0.5 + 0.5;

  float FXAA_SPAN_MAX = 8.0;
  float FXAA_REDUCE_MIN = 1.0/128.0;
  float FXAA_REDUCE_MUL = 1.0/8.0;

  vec2 texCoordOffset = 1.0 / texture_size;

  vec3 luma = vec3(0.299, 0.587, 0.114);
  float lumaTL = dot(luma, texture(uSampler, uv + (vec2(-1.0, -1.0) * texCoordOffset)).xyz);
  float lumaTR = dot(luma, texture(uSampler, uv + (vec2(1.0, -1.0) * texCoordOffset)).xyz);
  float lumaBL = dot(luma, texture(uSampler, uv + (vec2(-1.0, 1.0) * texCoordOffset)).xyz);
  float lumaBR = dot(luma, texture(uSampler, uv + (vec2(1.0, 1.0) * texCoordOffset)).xyz);
  float lumaM  = dot(luma, texture(uSampler, uv).xyz);

  vec2 dir;
  dir.x = -((lumaTL + lumaTR) - (lumaBL + lumaBR));
  dir.y = ((lumaTL + lumaBL) - (lumaTR + lumaBR));

  float dirReduce = max((lumaTL + lumaTR + lumaBL + lumaBR) * (FXAA_REDUCE_MUL * 0.25), FXAA_REDUCE_MIN);
  float inverseDirAdjustment = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);

  dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
    max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * inverseDirAdjustment));

  dir.x = dir.x * step(1.0, abs(dir.x));
  dir.y = dir.y * step(1.0, abs(dir.y));

  dir = dir * texCoordOffset;

  vec3 result1 = (1.0/2.0) * (
    texture(uSampler, uv + (dir * vec2(1.0/3.0 - 0.5))).xyz +
    texture(uSampler, uv + (dir * vec2(2.0/3.0 - 0.5))).xyz);

  vec3 result2 = result1 * (1.0/2.0) + (1.0/4.0) * (
    texture(uSampler, uv + (dir * vec2(0.0/3.0 - 0.5))).xyz +
    texture(uSampler, uv + (dir * vec2(3.0/3.0 - 0.5))).xyz);

  float lumaMin = min(lumaM, min(min(lumaTL, lumaTR), min(lumaBL, lumaBR)));
  float lumaMax = max(lumaM, max(max(lumaTL, lumaTR), max(lumaBL, lumaBR)));
  float lumaResult2 = dot(luma, result2);

  if (lumaResult2 < lumaMin || lumaResult2 > lumaMax) {
    color = vec4(result1, 1.0);
  } else {
    color = vec4(result2, 1.0);
  }
}
