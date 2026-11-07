#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace a2fo::shadows {

// Row-major storage, column vectors: result[row] = sum(m[row*4+col] * v[col]).
struct Vec3 { float x, y, z; };
struct Mat4 { std::array<float, 16> v; };
struct Bounds { Vec3 low{}, high{}; bool valid = false; };

inline bool finite(Vec3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

inline Mat4 identity() {
  return {{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}};
}

inline Mat4 transpose(Mat4 m) {
  Mat4 out{};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c)
      out.v[r * 4 + c] = m.v[c * 4 + r];
  return out;
}

inline Mat4 multiply(Mat4 a, Mat4 b) {
  Mat4 out{};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) {
      double sum = 0;
      for (int k = 0; k < 4; ++k)
        sum += static_cast<double>(a.v[r * 4 + k]) * b.v[k * 4 + c];
      out.v[r * 4 + c] = static_cast<float>(sum);
    }
  return out;
}

inline bool inverse(Mat4 m, Mat4 &out) {
  double a[4][8]{};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) {
      if (!std::isfinite(m.v[r * 4 + c])) return false;
      a[r][c] = m.v[r * 4 + c];
      a[r][c + 4] = r == c ? 1.0 : 0.0;
    }
  for (int c = 0; c < 4; ++c) {
    int pivot = c;
    for (int r = c + 1; r < 4; ++r)
      if (std::abs(a[r][c]) > std::abs(a[pivot][c])) pivot = r;
    if (!(std::abs(a[pivot][c]) > 1e-15) || !std::isfinite(a[pivot][c])) return false;
    for (int k = 0; k < 8; ++k) std::swap(a[c][k], a[pivot][k]);
    const double div = a[c][c];
    for (int k = 0; k < 8; ++k) a[c][k] /= div;
    for (int r = 0; r < 4; ++r) {
      if (r == c) continue;
      const double factor = a[r][c];
      for (int k = 0; k < 8; ++k) a[r][k] -= factor * a[c][k];
    }
  }
  Mat4 candidate{};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) {
      const double value = a[r][c + 4];
      if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) return false;
      candidate.v[r * 4 + c] = static_cast<float>(value);
    }
  out = candidate;
  return true;
}

inline bool transform(Mat4 m, Vec3 p, Vec3 &out) {
  if (!finite(p)) return false;
  double q[4]{};
  const double v[4] = {p.x, p.y, p.z, 1.0};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) {
      if (!std::isfinite(m.v[r * 4 + c])) return false;
      q[r] += static_cast<double>(m.v[r * 4 + c]) * v[c];
    }
  if (!std::isfinite(q[3]) || std::abs(q[3]) < 1e-15) return false;
  const double x = q[0] / q[3], y = q[1] / q[3], z = q[2] / q[3];
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
      std::abs(x) > std::numeric_limits<float>::max() ||
      std::abs(y) > std::numeric_limits<float>::max() ||
      std::abs(z) > std::numeric_limits<float>::max()) return false;
  out = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
  return true;
}

inline bool affine(Mat4 m, float tolerance = 1e-3f) {
  if (!std::isfinite(tolerance) || tolerance < 0) return false;
  for (float value : m.v) if (!std::isfinite(value)) return false;
  if (std::abs(m.v[12]) > tolerance || std::abs(m.v[13]) > tolerance ||
      std::abs(m.v[14]) > tolerance || std::abs(m.v[15] - 1.0f) > tolerance) return false;
  const double a = m.v[0], b = m.v[1], c = m.v[2];
  const double d = m.v[4], e = m.v[5], f = m.v[6];
  const double g = m.v[8], h = m.v[9], i = m.v[10];
  const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  return std::isfinite(det) && std::abs(det) > 1e-15;
}

inline bool infer_affine_transform(Mat4 projectionColumn, Mat4 viewColumn,
                                   Mat4 nativeClip, Mat4 &out) {
  for (float value : projectionColumn.v) if (!std::isfinite(value)) return false;
  for (float value : viewColumn.v) if (!std::isfinite(value)) return false;
  for (float value : nativeClip.v) if (!std::isfinite(value)) return false;

  // Solve (projection * view) * localToWorld = nativeClip without rounding
  // the product or inverse to float before camera/world translation cancels.
  double rows[4][8]{};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) {
      for (int k = 0; k < 4; ++k)
        rows[r][c] += static_cast<double>(projectionColumn.v[r * 4 + k]) *
                      viewColumn.v[k * 4 + c];
      rows[r][c + 4] = nativeClip.v[r * 4 + c];
    }
  for (int c = 0; c < 4; ++c) {
    int pivot = c;
    for (int r = c + 1; r < 4; ++r)
      if (std::abs(rows[r][c]) > std::abs(rows[pivot][c])) pivot = r;
    if (!(std::abs(rows[pivot][c]) > 1e-15) || !std::isfinite(rows[pivot][c])) return false;
    for (int k = 0; k < 8; ++k) std::swap(rows[c][k], rows[pivot][k]);
    const double divisor = rows[c][c];
    for (int k = 0; k < 8; ++k) rows[c][k] /= divisor;
    for (int r = 0; r < 4; ++r) {
      if (r == c) continue;
      const double factor = rows[r][c];
      for (int k = 0; k < 8; ++k) rows[r][k] -= factor * rows[c][k];
    }
  }
  Mat4 candidate{};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) {
      const double value = rows[r][c + 4];
      if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) return false;
      candidate.v[r * 4 + c] = static_cast<float>(value);
    }
  if (!affine(candidate)) return false;
  // The result is known affine now; remove only the accepted cancellation noise.
  candidate.v[12] = candidate.v[13] = candidate.v[14] = 0.0f;
  candidate.v[15] = 1.0f;
  out = candidate;
  return true;
}

inline bool include(Bounds &bounds, Vec3 p) {
  if (!finite(p)) return false;
  if (!bounds.valid) {
    bounds.low = bounds.high = p;
    bounds.valid = true;
  } else {
    bounds.low = {std::min(bounds.low.x, p.x), std::min(bounds.low.y, p.y), std::min(bounds.low.z, p.z)};
    bounds.high = {std::max(bounds.high.x, p.x), std::max(bounds.high.y, p.y), std::max(bounds.high.z, p.z)};
  }
  return true;
}

inline bool transformed_bounds(const Bounds &local, Mat4 localToWorld, Bounds &out) {
  if (!local.valid || !finite(local.low) || !finite(local.high) ||
      local.low.x > local.high.x || local.low.y > local.high.y || local.low.z > local.high.z ||
      !affine(localToWorld)) return false;
  Bounds result{};
  for (int n = 0; n < 8; ++n) {
    Vec3 corner{(n & 1) ? local.high.x : local.low.x,
                (n & 2) ? local.high.y : local.low.y,
                (n & 4) ? local.high.z : local.low.z};
    Vec3 world{};
    if (!transform(localToWorld, corner, world) || !include(result, world)) return false;
  }
  out = result;
  return true;
}

inline bool fit_shadow(const Bounds &bounds, Vec3 lightDirection, unsigned resolution,
                       Mat4 &worldToLightClip, Mat4 &worldToShadowUv) {
  if (!bounds.valid || !finite(bounds.low) || !finite(bounds.high) || !finite(lightDirection) ||
      bounds.low.x > bounds.high.x || bounds.low.y > bounds.high.y || bounds.low.z > bounds.high.z ||
      resolution < 512 || resolution > 4096 || (resolution & (resolution - 1)) != 0) return false;
  const double wx0 = lightDirection.x, wy0 = lightDirection.y, wz0 = lightDirection.z;
  const double len = std::hypot(wx0, wy0, wz0);
  if (!(len > 0) || !std::isfinite(len)) return false;
  const double wx = wx0 / len, wy = wy0 / len, wz = wz0 / len;
  const double ax = std::abs(wy) < .9 ? 0.0 : 1.0, ay = std::abs(wy) < .9 ? 1.0 : 0.0;
  double ux = ay * wz, uy = -ax * wz, uz = ax * wy - ay * wx;
  const double ulen = std::hypot(ux, uy, uz);
  if (!(ulen > 0) || !std::isfinite(ulen)) return false;
  ux /= ulen; uy /= ulen; uz /= ulen;
  const double vx = wy * uz - wz * uy, vy = wz * ux - wx * uz, vz = wx * uy - wy * ux;
  double lo[3] = {INFINITY, INFINITY, INFINITY}, hi[3] = {-INFINITY, -INFINITY, -INFINITY};
  for (int n = 0; n < 8; ++n) {
    const double x = (n & 1) ? bounds.high.x : bounds.low.x;
    const double y = (n & 2) ? bounds.high.y : bounds.low.y;
    const double z = (n & 4) ? bounds.high.z : bounds.low.z;
    const double p[3] = {ux * x + uy * y + uz * z,
                         vx * x + vy * y + vz * z,
                         wx * x + wy * y + wz * z};
    for (int j = 0; j < 3; ++j) { lo[j] = std::min(lo[j], p[j]); hi[j] = std::max(hi[j], p[j]); }
  }
  const double extent = std::max({hi[0] - lo[0], hi[1] - lo[1], 1.0}) * 1.02;
  const double width = std::exp2(std::ceil(std::log2(extent) * 4.0) / 4.0);
  const double texel = width / resolution;
  const double cu = std::floor(((lo[0] + hi[0]) * .5) / texel) * texel;
  const double cv = std::floor(((lo[1] + hi[1]) * .5) / texel) * texel;
  const double depthPad = std::max(1.0, (hi[2] - lo[2]) * .01);
  const double z0 = lo[2] - depthPad, range = hi[2] - lo[2] + 2.0 * depthPad;
  if (!std::isfinite(width) || !std::isfinite(texel) || !std::isfinite(cu) ||
      !std::isfinite(cv) || !std::isfinite(range) || width <= 0 || range <= 0 ||
      width > std::numeric_limits<float>::max() || range > std::numeric_limits<float>::max()) return false;
  // D3D clip z is [0,1]. The second matrix maps clip x/y to texture UV.
  const double clip[16] = {2*ux/width, 2*uy/width, 2*uz/width, -2*cu/width,
                           2*vx/width, 2*vy/width, 2*vz/width, -2*cv/width,
                           wx/range, wy/range, wz/range, -z0/range,
                           0, 0, 0, 1};
  Mat4 candidate{};
  for (int n = 0; n < 16; ++n) {
    if (!std::isfinite(clip[n]) || std::abs(clip[n]) > std::numeric_limits<float>::max()) return false;
    candidate.v[n] = static_cast<float>(clip[n]);
  }
  const Mat4 uvFromClip{{.5f, 0, 0, .5f, 0, -.5f, 0, .5f,
                         0, 0, 1, 0, 0, 0, 0, 1}};
  Mat4 uv = multiply(uvFromClip, candidate);
  for (float value : uv.v) if (!std::isfinite(value)) return false;
  worldToLightClip = candidate;
  worldToShadowUv = uv;
  return true;
}

} // namespace a2fo::shadows
