#include "rt.h"
#include "camera.h"
#include "hittable_list.h"
#include "material.h"
#include "sphere.h"
#include <chrono>
#include <cstring>

int main(int argc, char* argv[]) {
    bool save_ppm = false, save_png = false, save_bmp = false, save_jpg = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-ppm") == 0) save_ppm = true;
        else if (strcmp(argv[i], "-png") == 0) save_png = true;
        else if (strcmp(argv[i], "-bmp") == 0) save_bmp = true;
        else if (strcmp(argv[i], "-jpg") == 0) save_jpg = true;
    }
    if (!save_ppm && !save_png && !save_bmp && !save_jpg) save_png = true;

    std::clog << "Building scene...\n";

    // World
    hittable_list world;

    auto ground_material = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(make_shared<sphere>(point3(0,-1000,0), 1000, ground_material));

    const int sphere_count = 4;
    for (int a = -sphere_count; a < sphere_count; a++) {
        for (int b = -sphere_count; b < sphere_count; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9*random_double(), 0.2, b + 0.9*random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                shared_ptr<material> sphere_material;

                if (choose_mat < 0.8) {
                    auto albedo = color::random() * color::random();
                    sphere_material = make_shared<lambertian>(albedo);
                    world.add(make_shared<sphere>(center, 0.2, sphere_material));
                } else if (choose_mat < 0.95) {
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = make_shared<metal>(albedo, fuzz);
                    world.add(make_shared<sphere>(center, 0.2, sphere_material));
                } else {
                    sphere_material = make_shared<dielectric>(1.5);
                    world.add(make_shared<sphere>(center, 0.2, sphere_material));
                }
            }
        }
    }

    // Big spheres
    auto material1 = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, material1));

    auto material2 = make_shared<lambertian>(color(0.4, 0.2, 0.1));
    world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, material2));

    auto material3 = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, material3));

    std::clog << "Scene contains " << world.objects.size() << " objects\n";

    // Camera
    camera cam;
    cam.max_threads         = 8;
    cam.low_priority_mode   = true;
    cam.aspect_ratio        = 16.0 / 9.0;
    cam.image_width         = 1280;
    cam.samples_per_pixel   = 500;
    cam.max_depth           = 25;

    cam.vfov     = 20;
    cam.lookfrom = point3(13,2,3);
    cam.lookat   = point3(0,0,0);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 0.6;
    cam.focus_dist    = 10.0;

    cam.set_output_formats(save_ppm, save_png, save_bmp, save_jpg);

    // Render
    cam.render(world);

    return 0;
}
