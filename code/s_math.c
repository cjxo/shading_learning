function f32
radians(f32 x) {
	f32 result = x * 0.01745329251f;
	return(result);
}

function v3f
v3f_make(f32 x, f32 y, f32 z) {
	v3f result;
	result.x = x;
	result.y = y;
	result.z = z;
	return(result);
}

function v3f
v3f_add(v3f a, v3f b) {
	v3f result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	return(result);
}

function v3f
v3f_sub(v3f a, v3f b) {
	v3f result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	return(result);
}

function v3f
v3f_scale(v3f a, f32 s) {
	v3f result;
	result.x = a.x * s;
	result.y = a.y * s;
	result.z = a.z * s;
	return(result);
}

function f32
v3f_dot(v3f a, v3f b) {
	f32 result = a.x * b.x + a.y * b.y + a.z * b.z;
	return(result);
}

function v3f
v3f_cross(v3f a, v3f b) {
	v3f result;
	result.x = a.y * b.z - a.z * b.y;
	result.y = -(a.x * b.z - a.z * b.x);
	result.z = a.x * b.y - a.y * b.x;
	return(result);
}

function void
v3f_norm(v3f *a) {
	f32 imag = 1.0f / sqrtf(a->x * a->x + a->y * a->y + a->z * a->z);
	a->x *= imag;
	a->y *= imag;
	a->z *= imag;
}

function v4f
v4f_make(f32 x, f32 y, f32 z, f32 w) {
	v4f result;
	result.x = x;
	result.y = y;
	result.z = z;
	result.w = w;
	return(result);
}

function quat
quat_make(f32 s, f32 i, f32 j, f32 k) {
	quat result;
	result.real = s;
	result.i = i;
	result.j = j;
	result.k = k;
	return(result);
}

function quat
quat_add(quat a, quat b) {
	quat result;
	result.real = a.real + b.real;
	result.imaginary = v3f_add(a.imaginary, b.imaginary);
	return(result);
}

function quat
quat_sub(quat a, quat b) {
	quat result;
	result.real = a.real - b.real;
	result.imaginary = v3f_sub(a.imaginary, b.imaginary);
	return(result);
}

function quat
quat_conj(quat a) {
	quat result;
	result.real = a.real;
	result.imaginary = v3f_make(-a.imaginary.x, -a.imaginary.y, -a.imaginary.z);
	return(result);
}

function f32
quat_mag(quat a) {
	return sqrtf(a.real * a.real + v3f_dot(a.imaginary, a.imaginary));
}

function void
quat_norm(quat *a) {
	f32 imag = 1.0f / quat_mag(*a);
	a->x *= imag;
	a->y *= imag;
	a->z *= imag;
	a->w *= imag;
}

function quat
quat_mul(quat a, quat b) {
	quat result;
	result.real = a.real * b.real - v3f_dot(a.imaginary, b.imaginary);

	v3f left = v3f_scale(b.imaginary, a.real);
	v3f middle = v3f_scale(a.imaginary, b.real);
	result.imaginary = v3f_add(v3f_add(left, middle), v3f_cross(a.imaginary, b.imaginary));
	return(result);
}

function quat
quat_inv(quat a) {
	quat result;
	f32 mag_sq = a.real * a.real + v3f_dot(a.imaginary, a.imaginary);
	result.real = a.real / mag_sq;
	result.imaginary = v3f_scale(a.imaginary, 1.0f / mag_sq);
	return(result);
}

function quat
quat_make_rotate_around_axis(f32 angle_radians, v3f axis) {
	quat result;
	f32 c = cosf(angle_radians * 0.5f);
	f32 s = sinf(angle_radians * 0.5f);

	v3f_norm(&axis);
	result.real = c;
	result.imaginary = v3f_scale(axis, s);
	return(result);
}

function v3f
quat_rot_v3f(quat orient, v3f p) {
	quat quatp = quat_make(0.0f, p.x, p.y, p.z);
	
	return quat_mul(quat_mul(orient, quatp), quat_conj(orient)).imaginary;
}

function m44
m44_perspective_lh_z01(f32 fov_radians, f32 aspect_h_over_w, f32 near_plane, f32 far_plane) {
	f32 right = tanf(fov_radians * 0.5f) * near_plane;
	f32 left = -right;

	f32 top = right * aspect_h_over_w;
	f32 bottom = -top;

	m44 result = { 0 };
	result.m[0][0] = (2.0f * near_plane) / (right - left);
	result.m[2][0] = -(right + left) / (right - left);
	
	result.m[1][1] = (2.0f * near_plane) / (top - bottom);
	result.m[2][1] = -(top + bottom) / (top - bottom);
	
	result.m[2][2] = far_plane / (far_plane - near_plane);
	result.m[2][3] = 1.0f; 
	result.m[3][2] = -(near_plane * far_plane) / (far_plane - near_plane);

	return(result);
}
