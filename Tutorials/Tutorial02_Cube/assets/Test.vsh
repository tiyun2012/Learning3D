#define attribute in
#define varying out
#define texture2D texture
#define GL2
#define VERTEXSHADER

#define SHADER_NAME LitShader
varying vec2 vUV0_1;
varying vec3 vPositionW;
varying vec3 vNormalW;
attribute vec3 vertex_position;
attribute vec3 vertex_normal;
attribute vec4 vertex_tangent;
attribute vec2 vertex_texCoord0;
attribute vec2 vertex_texCoord1;
attribute vec4 vertex_color;
uniform mat4 matrix_viewProjection;
uniform mat4 matrix_model;
uniform mat3 matrix_normal;
vec3 dPositionW;
mat4 dModelMatrix;
mat3 dNormalMatrix;
vec2 getUv0() {
    return vertex_texCoord0;
}
uniform vec3 texture_diffuseMapTransform0;
uniform vec3 texture_diffuseMapTransform1;
mat4 getModelMatrix() {
    return matrix_model;
}
vec4 getPosition() {
    dModelMatrix = getModelMatrix();
    vec3 localPos = vertex_position;
    vec4 posW = dModelMatrix * vec4(localPos, 1.0);
    dPositionW = posW.xyz;
    vec4 screenPos;
    screenPos = matrix_viewProjection * posW;
    return screenPos;
}
vec3 getWorldPosition() {
    return dPositionW;
}
vec3 getNormal() {
    dNormalMatrix = matrix_normal;
    vec3 tempNormal = vertex_normal;
    return normalize(dNormalMatrix * tempNormal);
}
void main(void) {
    gl_Position = getPosition();
    gl_Position.x = float(RANDOM_ID);
    vPositionW = getWorldPosition();
    vNormalW = getNormal();
    vec2 uv0 = getUv0();
    vUV0_1 = vec2(dot(vec3(uv0, 1), texture_diffuseMapTransform0), dot(vec3(uv0, 1), texture_diffuseMapTransform1));
}