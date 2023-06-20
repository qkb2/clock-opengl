#version 330

uniform sampler2D textureMap0; 
uniform sampler2D textureMap1;
uniform float exponent;
uniform float specular;
uniform vec4 col1;
uniform vec4 col2;

out vec4 pixelColor; //Zmienna wyjsciowa fragment shadera. Zapisuje sie do niej ostateczny (prawie) kolor piksela

in vec2 iTexCoord0;
in vec4 ic; 
in vec4 n;
in vec4 l1;
in vec4 l2;
in vec4 v;

void main(void) {

	//Znormalizowane interpolowane wektory
	vec4 ml1 = normalize(l1);
	vec4 ml2 = normalize(l2);

	vec4 mn = normalize(n);
	vec4 mv = normalize(v);
	//Wektor odbity
	vec4 mr1 = reflect(-ml1, mn);
	vec4 mr2 = reflect(-ml2, mn);

	//Parametry powierzchni


	vec4 kd1 = col1*texture(textureMap0, iTexCoord0);
	vec4 ks1 = col1*texture(textureMap1, iTexCoord0);


	vec4 kd2 = col2*texture(textureMap0, iTexCoord0);
	vec4 ks2 = col2*texture(textureMap1, iTexCoord0);

	//Obliczenie modelu oświetlenia
	float nl1 = clamp(dot(mn, ml1), 0, 1);
	float nl2 = clamp(dot(mn, ml2), 0, 1);

	float rv1 = specular*pow(clamp(dot(mr1, mv), 0, 1), exponent);
	float rv2 = specular*pow(clamp(dot(mr2, mv), 0, 1), exponent);

	pixelColor= vec4(kd1.rgb*nl1 + kd2.rgb*nl2, kd1.a+kd2.a) + vec4(ks1.rgb*rv1 + ks2.rgb*rv2, 0);
}
