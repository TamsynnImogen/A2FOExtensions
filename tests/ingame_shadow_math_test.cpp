#include "../renderer/shadows/math.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace a2fo::shadows;

static bool near(float a, float b, float eps = 1e-4f) { return std::abs(a - b) <= eps; }

static Mat4 translation(float x, float y, float z) {
  Mat4 m = identity();
  m.v[3] = x; m.v[7] = y; m.v[11] = z;
  return m;
}

static void infer_transform_precision() {
  // D3D perspective, near=1 and far=100000, with camera/world offsets.
  const float depth = 100000.0f / 99999.0f;
  const Mat4 projection{{1, 0, 0, 0, 0, 1, 0, 0,
                         0, 0, depth, -depth, 0, 0, 1, 0}};
  const Mat4 rotationScale{{0, -2, 0, 0, 2, 0, 0, 0,
                            0, 0, 2, 0, 0, 0, 0, 1}};
  for (float distance : {1200.0f, 10000.0f, 100000.0f}) {
    const Mat4 view = translation(-distance, 340.0f, -distance * .75f);
    const Mat4 expected = translation(distance, -340.0f, distance * .75f);
    Mat4 recovered{};
    assert(infer_affine_transform(projection, view, projection, recovered));
    assert(recovered.v[12] == 0 && recovered.v[13] == 0 &&
           recovered.v[14] == 0 && recovered.v[15] == 1);
    assert(near(recovered.v[3], expected.v[3], .015625f));
    assert(near(recovered.v[7], expected.v[7], .015625f));
    assert(near(recovered.v[11], expected.v[11], .015625f));
    assert(near(recovered.v[0], 1) && near(recovered.v[5], 1) && near(recovered.v[10], 1));
    std::printf("shadow transform position error at %.0f: x=%g y=%g z=%g\n",
                distance, std::abs(recovered.v[3] - expected.v[3]),
                std::abs(recovered.v[7] - expected.v[7]),
                std::abs(recovered.v[11] - expected.v[11]));

    // The old float-rounded product/inverse accumulates a visible translation error.
    if (distance == 1200.0f) {
      Mat4 roundedInverse{};
      assert(inverse(multiply(projection, view), roundedInverse));
      const Mat4 rounded = multiply(roundedInverse, projection);
      const float oldError = std::abs(rounded.v[3] - expected.v[3]);
      const float newError = std::abs(recovered.v[3] - expected.v[3]);
      std::printf("shadow transform x error at 1200: rounded=%g double-solve=%g\n",
                  oldError, newError);
      assert(oldError > .01f && newError < oldError);
    }

    const Mat4 nativeClip = multiply(projection, rotationScale);
    const Mat4 modelExpected = multiply(expected, rotationScale);
    assert(infer_affine_transform(projection, view, nativeClip, recovered));
    assert(affine(recovered));
    for (int i = 0; i < 12; ++i)
      assert(near(recovered.v[i], modelExpected.v[i], .015625f));
    assert(recovered.v[12] == 0 && recovered.v[13] == 0 &&
           recovered.v[14] == 0 && recovered.v[15] == 1);
  }

  Mat4 out = identity(), bad = projection;
  bad.v[0] = std::numeric_limits<float>::quiet_NaN();
  assert(!infer_affine_transform(bad, identity(), projection, out));
  bad = projection; bad.v[0] = std::numeric_limits<float>::infinity();
  assert(!infer_affine_transform(projection, identity(), bad, out));
  bad = identity(); bad.v[3] = std::numeric_limits<float>::quiet_NaN();
  assert(!infer_affine_transform(projection, bad, projection, out));
  bad = identity(); bad.v[5] = 0;
  assert(!infer_affine_transform(projection, bad, projection, out));
  bad = projection; bad.v[0] = 0;
  assert(!infer_affine_transform(bad, identity(), projection, out));
  bad = projection; bad.v[12] += .01f;
  assert(!infer_affine_transform(projection, identity(), bad, out));
  assert(out.v == identity().v);
}

static void corners_inside(Bounds b, Mat4 clip, Mat4 uv) {
  for (int n = 0; n < 8; ++n) {
    Vec3 p{(n & 1) ? b.high.x : b.low.x, (n & 2) ? b.high.y : b.low.y,
           (n & 4) ? b.high.z : b.low.z};
    Vec3 c{}, t{};
    assert(transform(clip, p, c) && transform(uv, p, t));
    assert(c.x >= -1.0001f && c.x <= 1.0001f);
    assert(c.y >= -1.0001f && c.y <= 1.0001f);
    assert(c.z >= -.0001f && c.z <= 1.0001f);
    assert(t.x >= -.0001f && t.x <= 1.0001f);
    assert(t.y >= -.0001f && t.y <= 1.0001f);
    assert(near(t.x, .5f * c.x + .5f) && near(t.y, -.5f * c.y + .5f) && near(t.z, c.z));
  }
}

int main() {
  infer_transform_precision();
  const Mat4 id = identity();
  Mat4 nativeRow{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 7, -3, 2, 1}};
  Mat4 col = transpose(nativeRow);
  Vec3 q{};
  assert(transform(col, {1, 2, 3}, q) && near(q.x, 8) && near(q.y, -1) && near(q.z, 5));
  assert(multiply(id, col).v == col.v && multiply(col, id).v == col.v);
  assert(transpose(transpose(col)).v == col.v);
  assert(affine(col) && !affine(nativeRow));

  // Perspective divide and inverse round trip through nontrivial projective matrix.
  Mat4 perspective{{1.2f, .1f, 0, 0, 0, 1.4f, .2f, 0,
                    0, 0, 1.1f, -.5f, 0, 0, .3f, 1}};
  Mat4 inv{};
  assert(inverse(perspective, inv));
  Vec3 p{2, -1, 4}, projected{}, restored{};
  assert(transform(perspective, p, projected) && transform(inv, projected, restored));
  assert(near(restored.x, p.x) && near(restored.y, p.y) && near(restored.z, p.z));
  Mat4 singular = id;
  singular.v[5] = 0;
  assert(!inverse(singular, inv) && !affine(singular));
  Mat4 nonfinite = id;
  nonfinite.v[0] = std::numeric_limits<float>::quiet_NaN();
  assert(!inverse(nonfinite, inv) && !affine(nonfinite));
  nonfinite.v[0] = std::numeric_limits<float>::infinity();
  assert(!inverse(nonfinite, inv) && !affine(nonfinite));
  assert(!affine(perspective));
  Mat4 zeroW = id;
  zeroW.v[15] = 0;
  assert(!transform(zeroW, p, q));

  Bounds local{};
  assert(include(local, {-2, -1, -3}) && include(local, {1, 4, 2}));
  assert(!include(local, {NAN, 0, 0}));
  assert(near(local.low.x, -2) && near(local.high.y, 4));
  Mat4 rotatedScaled{{0, -2, 0, 5, 3, 0, 0, -6, 0, 0, -4, 7, 0, 0, 0, 1}};
  Bounds world{};
  assert(transformed_bounds(local, rotatedScaled, world));
  assert(near(world.low.x, -3) && near(world.high.x, 7));
  assert(near(world.low.y, -12) && near(world.high.y, -3));
  assert(near(world.low.z, -1) && near(world.high.z, 19));
  assert(!transformed_bounds(local, perspective, world));

  Mat4 clip{}, uv{};
  assert(fit_shadow(world, {1, -.8f, .3f}, 1024, clip, uv));
  corners_inside(world, clip, uv);
  Bounds planar{{-3, 0, -2}, {7, 0, 8}, true};
  assert(fit_shadow(planar, {0, -1, 0}, 512, clip, uv));
  corners_inside(planar, clip, uv);
  assert(!fit_shadow(world, {0, 0, 0}, 1024, clip, uv));
  assert(!fit_shadow(world, {0, 1, 0}, 1000, clip, uv));
  assert(!fit_shadow(world, {0, 1, 0}, 8192, clip, uv));
  assert(!fit_shadow(world, {NAN, 1, 0}, 1024, clip, uv));

  // One subtexel move leaves the light-space x/y translation fixed.
  Bounds stable{{-3, -2, -1}, {3, 2, 1}, true};
  Mat4 clip0{}, uv0{}, clip1{}, uv1{};
  assert(fit_shadow(stable, {0, 0, 1}, 1024, clip0, uv0));
  stable.low.x += .0001f; stable.high.x += .0001f;
  assert(fit_shadow(stable, {0, 0, 1}, 1024, clip1, uv1));
  assert(near(clip0.v[3], clip1.v[3]) && near(clip0.v[7], clip1.v[7]));

  Bounds huge{{-std::numeric_limits<float>::max(), -1, -1},
               {std::numeric_limits<float>::max(), 1, 1}, true};
  assert(!fit_shadow(huge, {0, 0, 1}, 1024, clip, uv));
}
