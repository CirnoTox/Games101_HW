#include <iostream>
#include <opencv2/opencv.hpp>
#include "global.hpp"
#include "Triangle.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "rasterizer.hpp"
#include "OBJ_Loader.h"

Eigen::Matrix4f get_view_matrix(Eigen::Vector3f eye_pos)
{
    Eigen::Matrix4f view = Eigen::Matrix4f::Identity();

    Eigen::Matrix4f translate;
    translate << 1,0,0,-eye_pos[0],
                 0,1,0,-eye_pos[1],
                 0,0,1,-eye_pos[2],
                 0,0,0,1;

    view = translate*view;

    return view;
}

Eigen::Matrix4f get_model_matrix(float angle)
{
    Eigen::Matrix4f rotation;
    angle = angle * MY_PI / 180.f;
    rotation << cos(angle), 0, sin(angle), 0,
                0, 1, 0, 0,
                -sin(angle), 0, cos(angle), 0,
                0, 0, 0, 1;

    Eigen::Matrix4f scale;
    scale << 2.5, 0, 0, 0,
              0, 2.5, 0, 0,
              0, 0, 2.5, 0,
              0, 0, 0, 1;

    Eigen::Matrix4f translate;
    translate << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1;

    return translate * rotation * scale;
}

Eigen::Matrix4f get_projection_matrix(float eye_fov, float aspect_ratio, float zNear, float zFar)
{
    // TODO: Use the same projection matrix from the previous assignments                  
        Eigen::Matrix4f projection = Eigen::Matrix4f::Identity();

    Eigen::Matrix4f persp_to_ortho = Eigen::Matrix4f::Identity();
    persp_to_ortho << zNear, 0,0,0,
                      0,zNear,0,0,
                      0,0,zNear + zFar, -(zNear*zFar),
                      0,0,1,0;

    float half_eye_fovY = eye_fov / 2 / 180.0 * MY_PI;
    zNear*=-1;
    float top = zNear * tan(half_eye_fovY);
    float bottom = -top;
    float right = aspect_ratio * top;
    float left = -right;

        Eigen::Matrix4f ortho_translate = Eigen::Matrix4f::Identity();
    ortho_translate << 1, 0, 0, -(right + left) / 2,
                       0, 1, 0, -(top + bottom) / 2,
                       0, 0, 1, -(zNear + zFar) / 2,
                       0, 0, 0, 1;

    Eigen::Matrix4f ortho_scale = Eigen::Matrix4f::Identity();
    ortho_scale << 2 / (right - left), 0,0,0,
                   0,2 / (top - bottom),0,0,
                   0,0,2 / (zNear - zFar), 0,
                   0,0,0,1;

    projection = ortho_scale * ortho_translate * persp_to_ortho*projection;

    return projection;
}

Eigen::Vector3f vertex_shader(const vertex_shader_payload& payload)
{
    return payload.position;
}

Eigen::Vector3f normal_fragment_shader(const fragment_shader_payload& payload)
{
    Eigen::Vector3f return_color = (payload.normal.head<3>().normalized() + Eigen::Vector3f(1.0f, 1.0f, 1.0f)) / 2.f;
    Eigen::Vector3f result;
    result << return_color.x() * 255, return_color.y() * 255, return_color.z() * 255;
    return result;
}

static Eigen::Vector3f reflect(const Eigen::Vector3f& vec, const Eigen::Vector3f& axis)
{
    auto costheta = vec.dot(axis);
    return (2 * costheta * axis - vec).normalized();
}

struct light
{
    Eigen::Vector3f position;
    Eigen::Vector3f intensity;
};

Eigen::Vector3f texture_fragment_shader(const fragment_shader_payload& payload)
{
    Eigen::Vector3f return_color = {0, 0, 0};
    if (payload.texture)
    {
        // TODO: Get the texture value at the texture coordinates of the current fragment
        return_color=payload.texture->getColor(payload.tex_coords.x(),payload.tex_coords.y());
    }
    Eigen::Vector3f texture_color;
    texture_color << return_color.x(), return_color.y(), return_color.z();

    Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005);
    Eigen::Vector3f kd = texture_color / 255.f;
    Eigen::Vector3f ks = Eigen::Vector3f(0.7937, 0.7937, 0.7937);

    auto l1 = light{{20, 20, 20}, {500, 500, 500}};
    auto l2 = light{{-20, 20, 0}, {500, 500, 500}};

    std::vector<light> lights = {l1, l2};
    Eigen::Vector3f amb_light_intensity{10, 10, 10};
    Eigen::Vector3f eye_pos{0, 0, 10};

    float p = 150;

    Eigen::Vector3f color = texture_color;
    Eigen::Vector3f point = payload.view_pos;
    Eigen::Vector3f normal = payload.normal;

    Eigen::Vector3f result_color = {0, 0, 0};

    auto ambient= ka.cwiseProduct(amb_light_intensity);
    for (auto& light : lights)
    {
        // TODO: For each light source in the code, calculate what the *ambient*, *diffuse*, and *specular* 
        // components are. Then, accumulate that result on the *result_color* object.
        auto arrived_energy=light.intensity[0]/std::pow((light.position-point).norm(),2);
        auto diffuse=Vector3f(0,0,0);
        auto ndotl=normal.dot((light.position-point).normalized());
        if(ndotl>0){
            diffuse=kd*arrived_energy*ndotl;
        }

        auto specular=Vector3f(0,0,0);
        auto h=((eye_pos-point).normalized()+(light.position-point).normalized())/
            ((eye_pos-point).normalized()+(light.position-point).normalized()).norm();
        auto ndoth=normal.dot(h.normalized());
        if(ndoth>0){
            specular=ks*arrived_energy*std::pow(ndoth,p);
        }
        result_color=result_color+diffuse+specular;
    }
    result_color+=ambient;

    return result_color * 255.f;
}

Eigen::Vector3f phong_fragment_shader(const fragment_shader_payload& payload)
{
    Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005);
    Eigen::Vector3f kd = payload.color;
    Eigen::Vector3f ks = Eigen::Vector3f(0.7937, 0.7937, 0.7937);

    auto l1 = light{{20, 20, 20}, {500, 500, 500}};
    auto l2 = light{{-20, 20, 0}, {500, 500, 500}};

    std::vector<light> lights = {l1, l2};
    Eigen::Vector3f amb_light_intensity{10, 10, 10};
    Eigen::Vector3f eye_pos{0, 0, 10};

    float p = 150;

    Eigen::Vector3f color = payload.color;
    Eigen::Vector3f point = payload.view_pos;
    Eigen::Vector3f normal = payload.normal;

    Eigen::Vector3f result_color = {0, 0, 0};
    auto ambient= ka.cwiseProduct(amb_light_intensity);
    for (auto& light : lights)
    {
        // TODO: For each light source in the code, calculate what the *ambient*, *diffuse*, and *specular* 
        // components are. Then, accumulate that result on the *result_color* object.
        auto arrived_energy=light.intensity[0]/std::pow((light.position-point).norm(),2);
        auto diffuse=Vector3f(0,0,0);
        auto ndotl=normal.dot((light.position-point).normalized());
        if(ndotl>0){
            diffuse=kd*arrived_energy*ndotl;
        }
        auto specular=Vector3f(0,0,0);
        auto h=((eye_pos-point).normalized()+(light.position-point).normalized())/
            ((eye_pos-point).normalized()+(light.position-point).normalized()).norm();
        auto ndoth=normal.dot(h.normalized());
        if(ndoth>0){
            //std::cout<<ndoth<<std::endl;
            specular=ks*arrived_energy*std::pow(ndoth,p);
        }
        result_color=result_color+diffuse+specular;
    }
    result_color+=ambient;
    return result_color * 255.f;
}



Eigen::Vector3f displacement_fragment_shader(const fragment_shader_payload& payload)
{
    
    Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005);
    Eigen::Vector3f kd = payload.color;
    Eigen::Vector3f ks = Eigen::Vector3f(0.7937, 0.7937, 0.7937);

    auto l1 = light{{20, 20, 20}, {500, 500, 500}};
    auto l2 = light{{-20, 20, 0}, {500, 500, 500}};

    std::vector<light> lights = {l1, l2};
    Eigen::Vector3f amb_light_intensity{10, 10, 10};
    Eigen::Vector3f eye_pos{0, 0, 10};

    float p = 150;

    Eigen::Vector3f color = payload.color; 
    Eigen::Vector3f point = payload.view_pos;
    Eigen::Vector3f normal = payload.normal;

    float kh = 0.2, kn = 0.1;
    
    auto n=normal;
    auto x=n.x();
    auto y=n.y();
    auto z=n.z();
    auto t=Vector3f(x*y/sqrt(x*x+z*z),sqrt(x*x+z*z),z*y/sqrt(x*x+z*z));
    auto b=n.cross(t);
    
    Matrix3f TBN;
    TBN<<t.x(),b.x(),n.x(),
       t.y(),b.y(),n.y(),
      t.z(),b.z(),n.z(); 

    auto u=payload.tex_coords[0];
    auto v=payload.tex_coords[1];
    auto w=payload.texture->width;
    auto h=payload.texture->height;
    auto texture_color=[&](auto u,auto v){
        return payload.texture->getColor(u,v).norm();
    };
    auto dU=kh*kn*(texture_color(u+1.0/w,v)-texture_color(u,v));
    auto dV=kh*kn*(texture_color(u,v+1.0/h)-texture_color(u,v));

    auto ln=Vector3f(-dU, -dV, 1);
    point+=kn*n*texture_color(u,v);
    n=(TBN*ln).normalized();
    // TODO: Implement displacement mapping here
    // Let n = normal = (x, y, z)
    // Vector t = (x*y/sqrt(x*x+z*z),sqrt(x*x+z*z),z*y/sqrt(x*x+z*z))
    // Vector b = n cross product t
    // Matrix TBN = [t b n]
    // dU = kh * kn * (h(u+1/w,v)-h(u,v))
    // dV = kh * kn * (h(u,v+1/h)-h(u,v))
    // Vector ln = (-dU, -dV, 1)
    // Position p = p + kn * n * h(u,v)
    // Normal n = normalize(TBN * ln)

    normal=n;
    Eigen::Vector3f result_color = {0, 0, 0};

    for (auto& light : lights)
    {
        auto arrived_energy=light.intensity[0]/std::pow((light.position-point).norm(),2);
        auto diffuse=Vector3f(0,0,0);
        auto ndotl=normal.dot((light.position-point).normalized());
        if(ndotl>0){
            diffuse=kd*arrived_energy*ndotl;
        }
        auto specular=Vector3f(0,0,0);
        auto h=((eye_pos-point).normalized()+(light.position-point).normalized())/
            ((eye_pos-point).normalized()+(light.position-point).normalized()).norm();
        auto ndoth=normal.dot(h.normalized());
        if(ndoth>0){
            //std::cout<<ndoth<<std::endl;
            specular=ks*arrived_energy*std::pow(ndoth,p);
        }

        // TODO: For each light source in the code, calculate what the *ambient*, *diffuse*, and *specular* 
        // components are. Then, accumulate that result on the *result_color* object.
        
        result_color=result_color+diffuse+specular;

    }
    //result_color/=2;
    auto ambient= ka.cwiseProduct(amb_light_intensity);
    result_color+=ambient;
    return result_color * 255.f;
}


Eigen::Vector3f bump_fragment_shader(const fragment_shader_payload& payload)
{
    
    Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005);
    Eigen::Vector3f kd = payload.color;
    Eigen::Vector3f ks = Eigen::Vector3f(0.7937, 0.7937, 0.7937);

    auto l1 = light{{20, 20, 20}, {500, 500, 500}};
    auto l2 = light{{-20, 20, 0}, {500, 500, 500}};

    std::vector<light> lights = {l1, l2};
    Eigen::Vector3f amb_light_intensity{10, 10, 10};
    Eigen::Vector3f eye_pos{0, 0, 10};

    float p = 150;

    Eigen::Vector3f color = payload.color; 
    Eigen::Vector3f point = payload.view_pos;
    Eigen::Vector3f normal = payload.normal;


    float kh = 0.2, kn = 0.1;
    
    auto x=normal.x();
    auto y=normal.y();
    auto z=normal.z();
    auto t=Vector3f(x*y/sqrt(x*x+z*z),sqrt(x*x+z*z),z*y/sqrt(x*x+z*z));
    auto b=normal.cross(t);
    
    Matrix3f TBN;
    TBN<<t.x(),b.x(),normal.x(),
       t.y(),b.y(),normal.y(),
      t.z(),b.z(),normal.z(); 

    auto u=payload.tex_coords.x();
    auto v=payload.tex_coords.y();
    auto w=payload.texture->width;
    auto h=payload.texture->height;
    auto texture_color=[&](auto u,auto v){
        return payload.texture->getColor(u,v).norm();
    };
    auto dU=kh*kn*(texture_color(u+1.0/w,v)-texture_color(u,v));
    auto dV=kh*kn*(texture_color(u,v+1.0/h)-texture_color(u,v));

    auto ln=Vector3f(-dU, -dV, 1);
    normal=(TBN*ln).normalized();
    
    // TODO: Implement bump mapping here
    // Let n = normal = (x, y, z)
    // Vector t = (x*y/sqrt(x*x+z*z),sqrt(x*x+z*z),z*y/sqrt(x*x+z*z))
    // Vector b = n cross product t
    // Matrix TBN = [t b n]
    // dU = kh * kn * (h(u+1/w,v)-h(u,v))
    // dV = kh * kn * (h(u,v+1/h)-h(u,v))
    // Vector ln = (-dU, -dV, 1)
    // Normal n = normalize(TBN * ln)


    Eigen::Vector3f result_color = {0, 0, 0};
    result_color = normal;

    return result_color * 255.f;
}

int main(int argc, const char** argv)
{
    std::vector<Triangle*> TriangleList;

    float angle = 140.0;
    bool command_line = false;

    std::string filename = "output.png";
    objl::Loader Loader;
    std::string obj_path = "../models/rock/";

    // Load .obj File
    bool loadout = Loader.LoadFile("../models/rock/rock.obj");
    for(auto mesh:Loader.LoadedMeshes)
    {
        for(int i=0;i<mesh.Vertices.size();i+=3)
        {
            Triangle* t = new Triangle();
            for(int j=0;j<3;j++)
            {
                t->setVertex(j,Vector4f(mesh.Vertices[i+j].Position.X,mesh.Vertices[i+j].Position.Y,mesh.Vertices[i+j].Position.Z,1.0));
                t->setNormal(j,Vector3f(mesh.Vertices[i+j].Normal.X,mesh.Vertices[i+j].Normal.Y,mesh.Vertices[i+j].Normal.Z));
                t->setTexCoord(j,Vector2f(mesh.Vertices[i+j].TextureCoordinate.X, mesh.Vertices[i+j].TextureCoordinate.Y));
            }
            TriangleList.push_back(t);
        }
    }

    rst::rasterizer r(700, 700);

    auto texture_path = "rock.png";
    r.set_texture(Texture(obj_path + texture_path));

    std::function<Eigen::Vector3f(fragment_shader_payload)> active_shader = phong_fragment_shader;

    if (argc >= 2)
    {
        command_line = true;
        filename = std::string(argv[1]);

        if (argc == 3 && std::string(argv[2]) == "texture")
        {
            std::cout << "Rasterizing using the texture shader\n";
            active_shader = texture_fragment_shader;
            texture_path = "rock.png";
            r.set_texture(Texture(obj_path + texture_path));
        }
        else if (argc == 3 && std::string(argv[2]) == "normal")
        {
            std::cout << "Rasterizing using the normal shader\n";
            active_shader = normal_fragment_shader;
        }
        else if (argc == 3 && std::string(argv[2]) == "phong")
        {
            std::cout << "Rasterizing using the phong shader\n";
            active_shader = phong_fragment_shader;
        }
        else if (argc == 3 && std::string(argv[2]) == "bump")
        {
            std::cout << "Rasterizing using the bump shader\n";
            active_shader = bump_fragment_shader;
        }
        else if (argc == 3 && std::string(argv[2]) == "displacement")
        {
            std::cout << "Rasterizing using the bump shader\n";
            active_shader = displacement_fragment_shader;
        }
    }

    Eigen::Vector3f eye_pos = {0,0,10};

    r.set_vertex_shader(vertex_shader);
    r.set_fragment_shader(active_shader);

    int key = 0;
    int frame_count = 0;

    if (command_line)
    {
        r.clear(rst::Buffers::Color | rst::Buffers::Depth);
        r.set_model(get_model_matrix(angle));
        r.set_view(get_view_matrix(eye_pos));
        r.set_projection(get_projection_matrix(45.0, 1, 0.1, 50));

        r.draw(TriangleList);
        cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
        image.convertTo(image, CV_8UC3, 1.0f);
        cv::cvtColor(image, image, cv::COLOR_RGB2BGR);

        cv::imwrite(filename, image);

        return 0;
    }

    while(key != 27)
    {
        r.clear(rst::Buffers::Color | rst::Buffers::Depth);

        r.set_model(get_model_matrix(angle));
        r.set_view(get_view_matrix(eye_pos));
        r.set_projection(get_projection_matrix(45.0, 1, 0.1, 50));

        //r.draw(pos_id, ind_id, col_id, rst::Primitive::Triangle);
        r.draw(TriangleList);
        cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
        image.convertTo(image, CV_8UC3, 1.0f);
        cv::cvtColor(image, image, cv::COLOR_RGB2BGR);

        cv::imshow("image", image);
        cv::imwrite(filename, image);
        key = cv::waitKey(10);

        if (key == 'a' )
        {
            angle -= 0.1;
        }
        else if (key == 'd')
        {
            angle += 0.1;
        }

    }
    return 0;
}
