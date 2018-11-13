#include<fstream>
#include<iostream>
#include "sphere.h"
#include "box.h"
#include "hitable_list.h"
#include "float.h"
#include "camera.h"
#include "random"
#include "material.h"
#include "surface_texture.h"
// Define STB_IMAGE, otherwise return error.
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"
#include "stb_image_write.h"
#include "aarect.h"
#include "constant_medium.h"
#include "bvh.h"
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#define random(a, b) (rand()%(b-a+1)+a)

using namespace std;

vec3 color(const ray &r, hitable *world, int depth) {
    hit_record rec;
    if (world->hit(r, 0.001, MAXFLOAT, rec)) {
        // Light after scattering
        ray scattered;
        // Light attenuation
        vec3 attenuation;
        // Calculate the color of the origin of light
        vec3 emitted = rec.mat_ptr->emitted(rec.u, rec.v, rec.p);
        if (depth < 50 && rec.mat_ptr->scatter(r, rec, attenuation, scattered)) {
            // regression
            return emitted + attenuation*color(scattered, world, depth+1);
        } else {
            return emitted;
        }
    } else {
        return vec3(0,0,0);
    }
}


// Final Scene Construction
hitable *final() {
    int nb = 10;
    hitable **list = new hitable*[3000];
    material *white = new lambertian( new constant_texture(vec3(0.73, 0.73, 0.73)) );
    material *ground = new lambertian( new constant_texture(vec3(0.48, 0.83, 0.53)) );
    int b = 0;
    int l = 0;
    for (int i = 0; i < nb; i++) {
        for (int j = 0; j < nb; j++) {
            float w = 100;
            float x0 = i*w;
            float z0 = j*w;
            float y0 = 0;
            float x1 = x0 + w;
            float y1 = 100*(drand48()+0.01);
            float z1 = z0 + w;
            cout << "("<<x0<<","<<y0<<","<<z0<<") ("<<x1<<","<<y1<<","<<z1<<")"<<endl;
            list[l++] = new box(vec3(x0, y0, z0), vec3(x1, y1, z1), ground);
        }
    }
    material *light = new diffuse_light( new constant_texture(vec3(7, 7, 7)) );
    list[l++] = new xz_rect(123, 423, 147, 412, 554, light);
    vec3 center(400, 400, 200);
    list[l++] = new moving_sphere(center, center+vec3(30, 0, 0), 0, 1, 50,
                                  new lambertian(new constant_texture(vec3(0.7, 0.3, 0.1))));
    list[l++] = new sphere(vec3(260, 150, 45), 50, new dielectric(1.5));
    list[l++] = new sphere(vec3(0, 150, 145), 50, new metal(vec3(0.8, 0.8, 0.9), 10.0));
    hitable *boundary = new sphere(vec3(360, 150, 145), 70, new dielectric(1.5));
    list[l++] = boundary;
    list[l++] = new constant_medium(boundary, 0.2, new constant_texture(vec3(0.2, 0.4, 0.9)));
    boundary = new sphere(vec3(0, 0, 0), 5000, new dielectric(1.5));
    list[l++] = new constant_medium(boundary, 0.0001, new constant_texture(vec3(1.0, 1.0, 1.0)));
    texture *pertext = new noise_texture(0.1);
    list[l++] =  new sphere(vec3(220,280, 300), 80, new lambertian( pertext ));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        list[l++] = new sphere(vec3(165*drand48()-100, 165*drand48()+270, 165*drand48()+395),10 , white);
    }
    cout<< "len(l) = " << l << endl;
    return new hitable_list(list,l);
}

inline vec3 de_nan(const vec3 &c)
{
    vec3 t = c;
    if(!(t[0]==t[0]))
        t[0]=0;
    if(!(t[1]==t[1]))
        t[1]=0;
    if(!(t[2]==t[2]))
        t[2]=0;
    return t;
}

const int processesCount=16;
int createProcess(){
    int id=0;
    for(int level = log2(processesCount);level>0; level--){
        if (fork()==0)
            id+=pow(2,level-1);
    }
    printf("\033[s\033[%dAProcess %d created\033[u",processesCount-id,id);
    return id;
}

// Main function. All detail for rendering are implemented in the header.
// Here are scene configuration as well as camera configuration
// Credits the author of the book P. Shirley for the scene that tests all features!
int main() {
    srand(0);
    string str = "";
    // pixel count (x,y)
    const int nx = 1024;
    const int ny = 1024;
    // Sampling Size
    const int ns = 5000;
    // Camera View
    vec3 lookfrom(228, 278, -800);
    vec3 lookat(278, 278, 0);
    float dist_to_focus = 10.0;
    float aperture = 0.0;
    float vfov = 40.0;
    camera cam(lookfrom, lookat, vec3(0, 1, 0), vfov, float(nx) / float(ny), aperture, dist_to_focus, 0.0, 1.0);

    hitable *world = final();

    random_device rd;
    int distribution_count,distribution_index;
    printf("Distribution count?: ");
    scanf("%d",&distribution_count);
    printf("Distribution index?: ");
    scanf("%d",&distribution_index);

    int distributionSliceBegin=ny/distribution_count*distribution_index;
    int distributionSliceRange=ny/distribution_count;

    ofstream mainFile("img.ppm");
    mainFile << "P3\n" << nx << " " << ny << "\n255\n";
    mainFile.close();
    printf("%s","\033[2J");
    for(int i=0;i<processesCount;i++){
        printf("%s","\n");
    }

    int workerID=createProcess();
    int workerBegin=distributionSliceRange/processesCount*(workerID+1)+distributionSliceBegin;
    int workerEnd=distributionSliceRange/processesCount*workerID+distributionSliceBegin;
    //if (workerID==processesCount-1) begin=ny;

    printf("\033[s\033[%dAWorker %d:                  row %d-row%d\033[u",processesCount-workerID,workerID,workerBegin,workerEnd);

    char fileName [10];
    sprintf(fileName,"imgChunk%d",workerID+distribution_index*processesCount);
    //Create the output file and then write to it.
    ofstream OutFile(fileName);

    for (int j = workerBegin - 1; j >= workerEnd; j--) {
        str = "";
        for (int i = 0; i < nx; i++) {
            vec3 col(0, 0, 0);
            printf("\033[s\033[%dAWorker %d: %d/%d\033[u",processesCount-workerID,workerID,(workerBegin-j-1)*nx+i,(workerBegin-workerEnd)*nx);
            cout.flush();
            for (int s = 0; s < ns; s++) {
                float u = float(i+drand48())/ float(nx);
                float v = float(j+drand48())/ float(ny);

                ray r = cam.get_ray(u, v);
                vec3 p = r.point_at_parameter(2.0);
                vec3 temp = color(r, world, 0);
                temp = de_nan(temp);
                col += temp;
            }

            // average the color
            col /= float(ns);
            col = vec3(sqrt(col[0]), sqrt(col[1]), sqrt(col[2]));

            int ir = int(255.99 * col[0]);
            int ig = int(255.99 * col[1]);
            int ib = int(255.99 * col[2]);
            // r,g,b value can be larger than 255. When over 255, default to % 255
            ir = ir>255?255:ir;
            ig = ig>255?255:ig;
            ib = ib>255?255:ib;

            string s = to_string(ir) + " " + to_string(ig) + " " + to_string(ib) + "\n";
            str += s;
        }

        OutFile << str;
    }

    printf("\033[s\033[%dA\033[KWorker %d: completed\033[u",processesCount-workerID,workerID);
    OutFile.close();            // Finish writing to file.

}

