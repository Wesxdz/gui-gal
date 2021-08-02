#version 400 core
in vec2 coords;
out vec4 color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform bool distortion = true;

uniform sampler2D screen;

// Distortion shader
// https://www.shadertoy.com/view/ls2GWc

const float PI=3.1415927;

#define NB 40.    // 25.
#define radius .3 // .4   max: 0.5

// === draw a circle (p,r) ===

float circle(vec2 p, float r) {
	
	p = p/r - 1.; r = p.x*p.y;
	// r = length(p)/r - 1.;

	return step(0.,r);
}

// === defines distortion ===

#define POLYNOMIAL 1
#if POLYNOMIAL
//const float P0=0.,P01=1.,P02=-1.; // P=x-xÂ² (0,0) / (.5,1/4) \ (1,0)
	const float P0=0.,P01=-0.5,P02=0.5;
	const float R=.5;	// resize the curve to [0,R]
	float F(float x) { return P0 + (P01/R)*x + (P02/(R*R))*x*x; }
#else
	const float R=PI/4.;
	float F(float x) { return .25*sin(-4.*x); }
#endif

// === scalar direct and reverse transforms ===

float f(float x) {
	float sx= sign(x), ax = abs(x); // We force odd symmetry => P0=0
	if (ax > R) return x;
	float dx = F(ax);
	return x + sx*dx; 
}

float invf(float x) {
	float sx= sign(x), ax = abs(x); // We force odd symmetry => P0=0
	if (ax > R) return x;

#if POLYNOMIAL
	// resize the curve to [0,R]
# if 0
	float B =.5*(1.+(P01/R))*(R*R/P02); // a=1, b/2, c
	float C =  (P0-ax)   *(R*R/P02);
	return sx*(-B + sign(B)*sqrt(B*B-C));  // -b' +- sqrt(b'2-c)
# else
	float B = .5*(1.+ (P01/R))*(R*R/P02) + ax; // a=1, -b/2, c
	float C = (P0 +ax*(P01/R))*(R*R/P02) + ax*ax;
	float dx = B - sign(B)*sqrt(B*B-C);  // -b' +- sqrt(b'2-c)
	return x - sx*dx;
# endif
#endif
}

// === vectorial direct and reverse transforms ===

vec2 disp(vec2 p, vec2 c) { // distorsion centre c size r
	float l=length(p-c);
	return c + (p-c)/l*f(l); // radial displacement
}
vec2 invdisp(vec2 p, vec2 c) { // inverse distorsion
	float l=length(p-c);
	return c + (p-c)/l*invf(l); // radial displacement
}

/// === draw a distorted pattern ===

float stiples(vec2 p, vec2 center, float n) {
	vec2 c, p2 = disp(p,center); 
	n *= .5;               // because domain range = [-1,1]
	p2 = n*p2+.5;
	if (distortion) { // --- distortion mode --- 
	    p2 = fract(p2)/n; // pos relative to a tile
	    c = vec2(.5/n);     
	} else {             // --- displacement mode --- 
		c = floor(p2)/n;
		c = invdisp(c,center);
		p2 = p;
	}

	
	return circle(p2-c,radius/n);
}

void main()
{
	color = texture(screen, coords);
    // vec4 baseColor = texture(screen, coords);
	// vec2 center = vec2(0.);
    // vec2 uv = 2.*(coords/600.0, - vec2(.9, .5));
    // // float v = stiples(uv, center, NB);
    // vec3 col = baseColor.xyz;
    // color = vec4(col, 1.); 
    // // vec4(0.0, 0.0, baseColor.z, 1.0); // Output just red component for testing!
    // color = baseColor;
}