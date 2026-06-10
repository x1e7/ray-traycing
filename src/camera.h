#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include "hittable.h"
#include "material.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

class camera {
public:
    int max_threads = 0;  // 0 = auto
    bool low_priority_mode = true;

    double aspect_ratio         = 16.0 / 9.0;
    int    image_width          = 1920;
    int    samples_per_pixel    = 500;
    int    max_depth            = 50;

    double vfov     = 20;
    point3 lookfrom = point3(13,2,3);
    point3 lookat   = point3(0,0,0);
    vec3   vup      = vec3(0,1,0);
    double defocus_angle = 0.6;
    double focus_dist = 10.0;

    void render(const hittable& world) {
        initialize();

        int image_size = image_width * image_height * 3;
        std::vector<unsigned char> image_data(image_size);

        std::clog << "Rendering " << image_width << "x" << image_height << "\n";

        #ifdef _WIN32
        if (low_priority_mode) {
            SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        }
        #endif

        int num_threads;
        if (max_threads > 0) {
            num_threads = max_threads;
        } else {
            num_threads = std::thread::hardware_concurrency();
            num_threads = std::max(1, num_threads - 2);
        }

        std::clog << "Using " << num_threads << " threads\n";

        std::vector<std::thread> threads;
        std::atomic<int> scanlines_completed{0};

        auto start = std::chrono::high_resolution_clock::now();

        auto render_range = [&](int start_y, int end_y) {
            for (int j = start_y; j < end_y; j++) {
                for (int i = 0; i < image_width; i++) {
                    color pixel_color(0, 0, 0);
                    for (int sample = 0; sample < samples_per_pixel; sample++) {
                        ray r = get_ray(i, j);
                        pixel_color += ray_color(r, max_depth, world);
                    }
                    int idx = (j * image_width + i) * 3;
                    write_color(&image_data[idx], pixel_samples_scale * pixel_color);
                }
                scanlines_completed++;
            }
        };

        int rows_per_thread = image_height / num_threads;
        for (int t = 0; t < num_threads; t++) {
            int start_y = t * rows_per_thread;
            int end_y = (t == num_threads - 1) ? image_height : start_y + rows_per_thread;
            threads.emplace_back(render_range, start_y, end_y);
        }

        int last_percent = -1;
        while (scanlines_completed < image_height) {
            int percent = (scanlines_completed * 100) / image_height;
            if (percent != last_percent) {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start);

                std::clog << "\r[" << std::string(percent / 2, '=')
                          << std::string(50 - percent / 2, ' ') << "] "
                          << percent << "% (" << scanlines_completed << "/"
                          << image_height << " rows) - " << elapsed.count() << "s elapsed"
                          << std::flush;
                last_percent = percent;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        for (auto& t : threads) {
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

        std::clog << "\n\n========================================\n";
        std::clog << "Render completed in " << duration.count() << " seconds\n";
        std::clog << "Estimated time per ray: ~"
                  << (duration.count() * 1000000.0) / ((long long)image_width * image_height * samples_per_pixel)
                  << " microseconds\n";
        std::clog << "========================================\n";

        save_images(image_data.data());
    }

    void set_output_formats(bool ppm, bool png, bool bmp, bool jpg) {
        save_ppm = ppm;
        save_png = png;
        save_bmp = bmp;
        save_jpg = jpg;
    }

private:
    int    image_height;
    double pixel_samples_scale;
    point3 center;
    point3 pixel00_loc;
    vec3   pixel_delta_u;
    vec3   pixel_delta_v;
    vec3   u, v, w;
    vec3   defocus_disk_u;
    vec3   defocus_disk_v;

    bool save_ppm = false;
    bool save_png = true;
    bool save_bmp = false;
    bool save_jpg = false;

    void initialize() {
        image_height = int(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        pixel_samples_scale = 1.0 / samples_per_pixel;

        center = lookfrom;

        auto theta = degrees_to_radians(vfov);
        auto h = std::tan(theta/2);
        auto viewport_height = 2 * h * focus_dist;
        auto viewport_width = viewport_height * (double(image_width)/image_height);

        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        vec3 viewport_u = viewport_width * u;
        vec3 viewport_v = viewport_height * -v;

        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

        auto viewport_upper_left = center - (focus_dist * w) - viewport_u/2 - viewport_v/2;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

        auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    ray get_ray(int i, int j) const {
        auto offset = sample_square();
        auto pixel_sample = pixel00_loc
                          + ((i + offset.x()) * pixel_delta_u)
                          + ((j + offset.y()) * pixel_delta_v);

        auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;
        return ray(ray_origin, ray_direction);
    }

    vec3 sample_square() const {
        return vec3(random_double() - 0.5, random_double() - 0.5, 0);
    }

    point3 defocus_disk_sample() const {
        auto p = random_in_unit_disk();
        return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }

    color ray_color(const ray& r, int depth, const hittable& world) const {
        if (depth <= 0)
            return color(0,0,0);

        hit_record rec;

        if (world.hit(r, interval(0.001, infinity), rec)) {
            ray scattered;
            color attenuation;
            if (rec.mat->scatter(r, rec, attenuation, scattered))
                return attenuation * ray_color(scattered, depth-1, world);
            return color(0,0,0);
        }

        vec3 unit_direction = unit_vector(r.direction());
        auto a = 0.5*(unit_direction.y() + 1.0);
        return (1.0-a)*color(1.0, 1.0, 1.0) + a*color(0.5, 0.7, 1.0);
    }

    void save_images(unsigned char* image_data) {
        #ifdef _WIN32
            system("mkdir output 2>nul");
        #else
            system("mkdir -p output");
        #endif

        if (save_ppm) {
            FILE* f = fopen("output/render.ppm", "w");
            fprintf(f, "P3\n%d %d\n255\n", image_width, image_height);
            for (int i = 0; i < image_width * image_height * 3; i += 3)
                fprintf(f, "%d %d %d\n", image_data[i], image_data[i+1], image_data[i+2]);
            fclose(f);
            std::clog << "\nSaved: output/render.ppm\n";
        }

        if (save_png) {
            stbi_write_png("output/render.png", image_width, image_height, 3,
                          image_data, image_width * 3);
            std::clog << "Saved: output/render.png\n";
        }

        if (save_bmp) {
            stbi_write_bmp("output/render.bmp", image_width, image_height, 3, image_data);
            std::clog << "Saved: output/render.bmp\n";
        }

        if (save_jpg) {
            stbi_write_jpg("output/render.jpg", image_width, image_height, 3,
                          image_data, 100);
            std::clog << "Saved: output/render.jpg\n";
        }
    }
};
