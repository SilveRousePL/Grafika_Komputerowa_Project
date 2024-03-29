#include "pch.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

// Klasa opisująca punkt
class Point3 {
public:
	float x, y, z;
	Point3() : x(0), y(0), z(0) {}
	Point3(float x, float y, float z) : x(x), y(y), z(z) {}
};

// Klasa opisująca źródło światła
class Light {
public:
	Point3 position, specular, diffuse, ambient;
	Light(Point3 position, Point3 specular, Point3 diffuse, Point3 ambient) {
		this->position = position;
		this->specular = specular;
		this->diffuse = diffuse;
		this->ambient = ambient;
	}
	Light() {
		this->position = Point3(0, 0, 0);
		this->specular = Point3(0, 0, 0);
		this->diffuse = Point3(0, 0, 0);
		this->ambient = Point3(0, 0, 0);
	}
};

// Klasa opisująca sferę
class Sphere {
public:
	Point3 position, specular, diffuse, ambient;
	float radius, specularhininess;
	Sphere(Point3 position, float radius, Point3 specular, Point3 diffuse, Point3 ambient, float specularhininess) {
		this->position = position;
		this->radius = radius;
		this->specular = specular;
		this->diffuse = diffuse;
		this->ambient = ambient;
		this->specularhininess = specularhininess;
	}
	Sphere() {
		this->position = Point3(0, 0, 0);
		this->radius = 0.0;
		this->specular = Point3(0, 0, 0);
		this->diffuse = Point3(0, 0, 0);
		this->ambient = Point3(0, 0, 0);
		this->specularhininess = 0.0;
	}
};

// Dane wczytywane z pliku
std::vector<Light> light;
std::vector<Sphere> sphere;
Point3 background(0.0, 0.0, 0.0);
Point3 global(0.0, 0.0, 0.0);
Point3 viewer_vec(0.0, 0.0, 1.0);
float viewport_size = 15.0;
int window_size_x, window_size_y;
int max_depth = 2;

// Typ statusu
enum Status {
	NO_INTERSECTION,
	INTERSECTION,
	LIGHT_SOURCE
};

// Klasa opisująca punkt na sferze
class SpherePoint {
public:
	Point3 point;
	int sphere_index;
	int light_index;
	Status status;
	SpherePoint(Point3 point, int sphere_index, int light_index, Status status) {
		this->point = point;
		this->sphere_index = sphere_index;
		this->light_index = light_index;
		this->status = status;
	}
	SpherePoint() {
		this->point = Point3(0.0, 0.0, 0.0);
		this->sphere_index = 0;
		this->light_index = 0;
		this->status = NO_INTERSECTION;
	}
};

// Funkcja oblicza iloczyn skalarny wektorów
float dotProduct(Point3 &p1, Point3 &p2) {
	return p1.x * p2.x + p1.y * p2.y + p1.z * p2.z;
}

// Funkcja normalizująca wektor
Point3 normalization(Point3 &p) {
	Point3 result = p;
	float d = 0.0;
	d += result.x * result.x;
	d += result.y * result.y;
	d += result.z * result.z;
	d = sqrt(d);
	if (d > 0.0) {
		result.x /= d;
		result.y /= d;
		result.z /= d;
	}
	return result;
}

/*
	Funkcja Normal() służy do wyliczania składowych wektora normalenego do powierzchni obiekt w punkcie q.
	Stopień skomplikowania algorytmu wyliczania wektora normalnego zależy oczywiście od tego jaki to jest obiekt.
	Dla najprostszych w sensie opisu matematycznego obiektów n.p. kwadryk, wektor normalny można obliczyć analitycznie
	i uzyskany wynik wpisać w kod funkcji. Dla obiektów o bardziej skomlikowanym modelu trzeba stosować niestety inne metody.
*/
// Funkcja tworzy wektor normalny do powierzchni danej sfery w punkcie q
Point3 normal(Point3 &q, int sphere_index) {
	Point3 result(q.x - sphere[sphere_index].position.x, q.y - sphere[sphere_index].position.y, q.z - sphere[sphere_index].position.z);
	return normalization(result);
}

/*
	Funkcja Reflect() przewidziana została do wyznaczenia wektora jednostkowego opisującego kierunek kolejnego śledzonego promienia.
	Promień ten powstaje w wyniku odbicia promienia wychodzącego z punktu p i biegnącego do punktu q na powierzchni obiektu.
	Dla wyznaczenia poszukiwanego wektora należy znać więc kierunek promienia wyznaczony przez parę punktów p i q
	oraz wektor normalny do powierzchni w punkcie q. Zakładając, że dla śledzonego promienia słuszna jest zasada mówiąca,
	że kąt odbicia promienia jest równy kątowi pod jakim promień pada na analizowany punkt,
	poszukiwany wektor kierunku odbicia można wyliczyć podobnie jak wektor odbicia światła R dla opisanego wyżej modelu Phonga.
*/
// Funkcja obliczająca kierunek odbicia promienia w punkcie q
Point3 reflect(Point3 &p, Point3 &q, Point3 &n) {
	Point3 direct_vec(p.x - q.x, p.y - q.y, p.z - q.z);
	direct_vec = normalization(direct_vec);
	float n_dot_l = dotProduct(direct_vec, n);
	Point3 result(2 * (n_dot_l)* n.x - direct_vec.x, 2 * (n_dot_l)* n.y - direct_vec.y, 2 * (n_dot_l)* n.z - direct_vec.z);

	if (result.x * result.x + result.y * result.y + result.z * result.z > 1.0)
		return normalization(result);
	else return result;
}

/*
	Funkcja intercest() ma za zadanie wyznaczenie współrzędnych punktu przecięcia z najbliższym obiektem sceny,
	znajdujacym sie na drodze śledzonego promienia i ustawienie odpowiedniej watrości zmiennej status.
	Argumentami funkcji są punkt p bedący poczatkiem promienia i kierunek promienia d.
	Zmienna status powinna przyjąc jedną z trzech możliwych wartości:
	- light_source, gdy śledzony promień trafi w źródło swiatła,
	- no_ intesection gdy promień nie trafia w żaden obiekt sceny,
	- intersection w przypadku przecięcia z obiektem.
*/
// Funkcja wyznacza współrzędne punktu przecięcia z najbliższym obiektem sceny,
// znajdującym się na drodze śledzonego promienia
SpherePoint intercest(Point3 &start, Point3 &vec) {
	SpherePoint result;
	for (int i = 0; i < light.size(); ++i) {
		float x = light[i].position.x - start.x;
		float y = light[i].position.y - start.y;
		float z = light[i].position.z - start.z;
		if ((x / vec.x) == (y / vec.y) && (y / vec.y) == (z / vec.z)) {
			result.point = light[i].position;
			result.status = LIGHT_SOURCE;
			return result;
		}
	}
	for (int i = 0; i < sphere.size(); ++i) {
		float a = vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
		float b = 2 * ((start.x - sphere[i].position.x) * vec.x + (start.y
			- sphere[i].position.y) * vec.y +
			(start.z - sphere[i].position.z) * vec.z);
		float c = (start.x * start.x + start.y * start.y + start.z * start.z)
			+ (sphere[i].position.x * sphere[i].position.x + sphere[i].position.y * sphere[i].position.y
				+ sphere[i].position.z * sphere[i].position.z)
			- 2 * (start.x * sphere[i].position.x + start.y * sphere[i].position.y +
				start.z * sphere[i].position.z)
			- sphere[i].radius * sphere[i].radius;
		float delta = b * b - 4 * a * c;
		if (delta >= 0) {
			float r = (-b - sqrt(delta)) / (2 * a);
			if (r > 0) {
				result.point.x = start.x + r * vec.x;
				result.point.y = start.y + r * vec.y;
				result.point.z = start.z + r * vec.z;
				result.sphere_index = i;
				result.status = INTERSECTION;
				break;
			}
		}
	}
	return result;
}

//q - punkt na sferze
//n - wektor normalny do powierzchni
//sphere_index - nr sfery
/*
	Zadaniem funkcji Phong() jest wyznaczenie oświetlenia lokalnego punktu q.
	Oświetlenie to jest sumą oświetleń pochodzących od wszystkich źródeł, które sa bezpośrednio widoczne z analizowanego punktu.
	Obliczenie oświetlenia polega na użyciu modelu Phonga, czyli zastosowaniu znanych już z ćwiczenia 5 wzorów w postaci:
*/
// Funkcja oblicza oświetlenie punktu na powierzchni sfery używając modelu Phonga
Point3 phong(Point3 &q, Point3 &n, int sphere_index) {
	Point3 color(0.0, 0.0, 0.0);

	for (int i = 0; i < light.size(); ++i) {
		// Wektor wskazujący źródło światła
		Point3 light_vec(light[i].position.x - q.x, light[i].position.y - q.y, light[i].position.z - q.z);
		// Normalizacja
		light_vec = normalization(light_vec);
		// Iloczyn skalarny N x L
		float n_dot_l = dotProduct(light_vec, n); // Zmienna pomocnicza

		// Wektor kierunku odbicia światła
		Point3 reflection_vec(2 * (n_dot_l)* n.x - light_vec.x, 2 * (n_dot_l)* n.y - light_vec.y, 2 * (n_dot_l)* n.z - light_vec.z);
		// Normalizacja
		reflection_vec = normalization(reflection_vec);
		// Iloczyn skalarny V x R
		float v_dot_r = dotProduct(viewer_vec, reflection_vec); // Zmienna pomocnicza

		if (v_dot_r < 0) // obserwator nie widzi oświetlanego punktu
			v_dot_r = 0;

		// Sprawdzenie czy punkt na powierzchni sfery jest oświetlany przez źródło
		if (n_dot_l > 0) { // Punkt jest oświetlany (oświetlenie wyliczane jest ze wzorów dla modelu Phonga)
			color.x +=
				(sphere[sphere_index].diffuse.x * light[i].diffuse.x * n_dot_l +
					sphere[sphere_index].specular.x * light[i].specular.x * pow(v_dot_r, sphere[sphere_index].specularhininess) +
					sphere[sphere_index].ambient.x * light[i].ambient.x + sphere[sphere_index].ambient.x * global.x);
			color.y +=
				(sphere[sphere_index].diffuse.y * light[i].diffuse.y * n_dot_l +
					sphere[sphere_index].specular.y * light[i].specular.y * pow(v_dot_r, sphere[sphere_index].specularhininess) +
					sphere[sphere_index].ambient.y * light[i].ambient.y + sphere[sphere_index].ambient.y * global.y);
			color.z +=
				(sphere[sphere_index].diffuse.z * light[i].diffuse.z * n_dot_l +
					sphere[sphere_index].specular.z * light[i].specular.z * pow(v_dot_r, sphere[sphere_index].specularhininess) +
					sphere[sphere_index].ambient.z * light[i].ambient.z + sphere[sphere_index].ambient.z * global.z);
		}
		else { // Punkt nie jest oświetlany (uwzględniane jest tylko światło rozproszone)
			color.x += (sphere[sphere_index].ambient.x) * global.x;
			color.y += (sphere[sphere_index].ambient.y) * global.y;
			color.z += (sphere[sphere_index].ambient.z) * global.z;
		}
	}
	return color;
}

/*
	Funkcja oblicza punkt przecięcia promienia i powierzchni sfery
	Argument "start" jest punktem początkowym promienia, "direction" 
	wektorem opisującym kierunek biegu promienia, a "step" jest licznikiem
	odbić promienia. Funkcja zwraca kolor jaki obliczyła.
*/
Point3 trace(Point3 &start, Point3 &direction, int step) {
	Point3 normal_vec; // Wektor znormalizowany
	Point3 reflect_vec; // Wektor odbity od powierzchni

	Point3 local(0.0, 0.0, 0.0); // Światło lokalnie w tej funkcji (zwracane przez funkcję trace)
	Point3 reflected(0.0, 0.0, 0.0); // Światło odbite (połączenia wyników trace)

	if (step > max_depth) { // Jeśli przekroczono max. liczbę odbić
		return background;
	}

	// Obliczenie punktu przecięcia promienia i obiektu sceny
	SpherePoint q = intercest(start, direction);
	Point3 q_vec(q.point.x, q.point.y, q.point.z); // Wektor normalny do powierzchni obiektu

	// Jeśli trafiono źródło światła
	if (q.status == LIGHT_SOURCE) {
		local.x = light[q.light_index].specular.x;
		local.y = light[q.light_index].specular.y;
		local.z = light[q.light_index].specular.z;
		return local;
	}
	// Jeśli nic nie zostało trafione
	if (q.status == NO_INTERSECTION) {
		return background;
	}

	//wyliczenie wektora znormalizowanego do powierzchni
	normal_vec = normal(q_vec, q.sphere_index);

	//wyliczenie promienia odbitego
	reflect_vec = reflect(start, q_vec, normal_vec);

	//obliczenie koloru punktu
	// q_vec - punkt na sferze,
	// normal_vec - wektor znormalizowany(q_vec) dla danej sfery,
	// q.sphere_index - indeks sfery
	local = phong(q_vec, normal_vec, q.sphere_index);

	// Obliczenie reszty oświetlenia dla punktu q_vec
	reflected = trace(q_vec, reflect_vec, step + 1);

	// Obliczenie całkowitego oświetlenia dla q_vec
	local.x += reflected.x;
	local.y += reflected.y;
	local.z += reflected.z;

	return local;
}

// Funkcja rysująca obraz oświetlonej sceny
void display() {
	glClear(GL_COLOR_BUFFER_BIT);
	glFlush();

	// połowa rozmiaru obrazu w pikselach
	int window_size_half_x = window_size_x / 2;
	int window_size_half_y = window_size_y / 2;

	// rysowanie pikseli od lewego górnego narożnika do prawego dolnego narożnika
	for (int y = window_size_half_y; y > -window_size_half_y; --y) { // pozycja rysowanego piksela "całkowitoliczbowa" (y)
		for (int x = -window_size_half_x; x < window_size_half_x; ++x) { // pozycja rysowanego piksela "całkowitoliczbowa" (x)
			// pozycja rysowanego piksela "zmiennoprzecinkowa" 
			float x_fl = (float)x / (window_size_x / viewport_size);
			float y_fl = (float)y / (window_size_y / viewport_size);

			// przeliczenie pozycji(x,y) w pikselach na pozycję "zmiennoprzecinkową" w oknie obserwatora
			Point3 starting_point(x_fl, y_fl, viewport_size);
			Point3 starting_directions(0.0, 0.0, -1.0);

			// wyznaczenie początku śledzonego promienia dla rysowanego piksela
			// zmienna określająca, czy sfera została przecięta przez
			Point3 color = trace(starting_point, starting_directions, 0);
			GLubyte pixel[1][1][3]; // składowe koloru rysowanego piksela

			// obliczenie punktu przecięcia ze sferą
			// konwersja wartości wyliczonego oświetlenia na liczby od 0 do 255
			if (color.x > 1)            // składowa czerwona R
				pixel[0][0][0] = 255;
			else
				pixel[0][0][0] = color.x * 255;
			if (color.y > 1)            // składowa zielona G
				pixel[0][0][1] = 255;
			else
				pixel[0][0][1] = color.y * 255;
			if (color.z > 1)            // składowa niebieska B
				pixel[0][0][2] = 255;
			else
				pixel[0][0][2] = color.z * 255;
			glRasterPos3f(x_fl, y_fl, 0);
			// inkrementacja pozycji rastrowej dla rysowania piksela
			glDrawPixels(1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
		}
		// Rysuj po 10 linii w jednym kroku
		if (!(y % 10)) glFlush();
	}
	glFlush();
}

// Funkcja odczytująca plik z parametrami sceny
void readFile(std::string path) {
	std::fstream file(path, std::ios::in);
	std::string line, type;
	if (!file.is_open()) {
		std::cout << "Wczytywanie pliku " << path << " nieudane!";
	}
	while (getline(file, line)) {
		std::istringstream iss(line);
		iss >> type;
		if (!type.compare("dimensions")) {
			iss >> window_size_x >> window_size_y;
		}
		else if (!type.compare("background"))
			iss >> background.x >> background.y >> background.z;
		else if (!type.compare("global"))
			iss >> global.x >> global.y >> global.z;
		else if (!type.compare("viewer"))
			iss >> viewer_vec.x >> viewer_vec.y >> viewer_vec.z;
		else if (!type.compare("viewport_size"))
			iss >> viewport_size;
		else if (!type.compare("max_depth"))
			iss >> max_depth;
		else if (!type.compare("sphere")) {
			Sphere loaded_sphere;
			iss >> loaded_sphere.radius >> loaded_sphere.position.x >> loaded_sphere.position.y >> loaded_sphere.position.z;
			iss >> loaded_sphere.specular.x >> loaded_sphere.specular.y >> loaded_sphere.specular.z;
			iss >> loaded_sphere.diffuse.x >> loaded_sphere.diffuse.y >> loaded_sphere.diffuse.z;
			iss >> loaded_sphere.ambient.x >> loaded_sphere.ambient.y >> loaded_sphere.ambient.z;
			iss >> loaded_sphere.specularhininess;
			sphere.push_back(loaded_sphere);
		}
		else if (!type.compare("source")) {
			Light loaded_light;
			iss >> loaded_light.position.x >> loaded_light.position.y >> loaded_light.position.z;
			iss >> loaded_light.specular.x >> loaded_light.specular.y >> loaded_light.specular.z;
			iss >> loaded_light.diffuse.x >> loaded_light.diffuse.y >> loaded_light.diffuse.z;
			iss >> loaded_light.ambient.x >> loaded_light.ambient.y >> loaded_light.ambient.z;
			light.push_back(loaded_light);
		}
	}
	file.close();
}

// Funkcja inicjalizująca definiująca sposób rzutowania
void myinit() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-viewport_size / 2, viewport_size / 2, -viewport_size / 2, viewport_size / 2, -viewport_size / 2, viewport_size / 2);
	glMatrixMode(GL_MODELVIEW);
}

int main() {
	std::string filename = "scene.txt";
	std::cout << "Prosze podac nazwe pliku: ";
	std::cin >> filename;
	readFile(filename);
	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGBA);
	glutInitWindowSize(window_size_x, window_size_y);
	glutCreateWindow("Recursive Ray Tracing - Projekt");
	myinit();
	glutDisplayFunc(display);
	glutMainLoop();
}
