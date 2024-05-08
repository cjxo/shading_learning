#if !defined(S_MATH_H)
#define S_MATH_H

typedef union {
    struct {
        f32 x, y, z;
    };
    struct {
        f32 r, g, b;
    };
    f32 v[3];
} v3f;

typedef union {
    struct {
        f32 x, y, z, w;
    };
    struct {
        f32 r, g, b, a;
    };
	struct {
		v3f xyz;
		f32 __unused_a;
	};
	struct {
		f32 real;
		v3f imaginary;
	};
	struct {
		f32 s;
		f32 i, j, k;
	};
    f32 v[4];
} v4f;

typedef union {
	v4f rows[4];
	f32 m[4][4];
} m44;

typedef v4f quat;

#define pi_f32 3.14159f
#define pi_half_f32 (pi_f32*0.5f)
function f32 radians(f32 x);

// V3s
function v3f v3f_make(f32 x, f32 y, f32 z);
function v3f v3f_add(v3f a, v3f b);
function v3f v3f_sub(v3f a, v3f b);
function v3f v3f_scale(v3f a, f32 s);
function f32 v3f_dot(v3f a, v3f b);
function v3f v3f_cross(v3f a, v3f b);
function void v3f_norm(v3f *a);

// V4s
function v4f v4f_make(f32 x, f32 y, f32 z, f32 w);
function quat quat_make(f32 s, f32 i, f32 j, f32 k);
function quat quat_identity(void);
function quat quat_add(quat a, quat b);
function quat quat_sub(quat a, quat b);
function quat quat_conj(quat a);
function f32 quat_mag(quat a);
function void quat_norm(quat *a);
function quat quat_mul(quat a, quat b);
function quat quat_inv(quat a);
function quat quat_make_rotate_around_axis(f32 angle_radians, v3f axis);

function m44 m44_perspective_lh_z01(f32 fov_radians, f32 aspect_h_over_w, f32 near_plane, f32 far_plane);

#endif
