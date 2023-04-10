#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "window.hpp"
#include "navigation/camera.hpp"
#include "shader/shader_exception.hpp"

#include "geometries/sphere.hpp"
#include "geometries/cube.hpp"
#include "geometries/gizmo.hpp"
#include "geometries/grid_lines.hpp"

#include "render/renderer.hpp"
#include "render/light.hpp"

using namespace geometry;

int main() {
  ////////////////////////////////////////////////
  // Window & camera
  ////////////////////////////////////////////////

  // glfw window
  Window window("FPS game");

  if (window.is_null()) {
    std::cout << "Failed to create window or OpenGL context" << "\n";
    return 1;
  }

  // initialize glad before calling gl functions
  window.make_context();
  if (!gladLoadGL()) {
    std::cout << "Failed to load Glad (OpenGL)" << "\n";
    window.destroy();
    return 1;
  } else {
    std::cout << "Opengl version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
  }

  // camera
  float cam_y = 10;
  glm::vec3 position_cam(25, cam_y, 50);
  glm::vec3 direction_cam(0, -0.5, -1);
  glm::vec3 up_cam(0, 1, 0);
  Camera camera(position_cam, direction_cam, up_cam);

  // transformation matrices
  float near = 0.001,
        far = 100.0;
  float aspect_ratio = (float) window.width / (float) window.height;
  glm::mat4 projection3d = glm::perspective(glm::radians(camera.fov), aspect_ratio, near, far);
  glm::mat4 view = camera.get_view();

  ////////////////////////////////////////////////
  // Renderers
  ////////////////////////////////////////////////

  // create & install vertex & fragment shaders on GPU
  Program program_basic("assets/shaders/basic.vert", "assets/shaders/basic.frag");
  Program program_phong("assets/shaders/phong.vert", "assets/shaders/phong.frag");

  if (program_basic.has_failed() || program_phong.has_failed()) {
    window.destroy();
    throw ShaderException();
  }

  // grid & gizmo for debugging
  Renderer gizmo(program_basic, Gizmo(), Attributes::get({"position"}));
  Renderer grid(program_basic, GridLines(50), Attributes::get({"position"}));
  Renderer cubes(program_basic, Cube(), Attributes::get({"position"}, 8));

  // spheres
  Renderer spheres(program_phong, Sphere(1.0, 32, 32), Attributes::get({"position", "normal"}));

  // enable depth test & blending & stencil test (for outlines)
  glEnable(GL_DEPTH_TEST);
  glm::vec4 background(0.0f, 0.0f, 0.0f, 1.0f);
  glClearColor(background.r, background.g, background.b, background.a);

  // backface (where winding order is CW) not rendered (boost fps)
  glEnable(GL_CULL_FACE);

  // take this line as a ref. to calculate initial fps (not `glfwInit()`)
  window.init_timer();

  ////////////////////////////////////////////////
  // Objects
  ////////////////////////////////////////////////

  // light cubes (scaling then translation)
  Light light(glm::vec3(5.5f, 3.0f, 4.0f), glm::vec3(1));
  glm::mat4 translate_light = glm::translate(glm::mat4(1), light.position);
  glm::mat4 scale_light = glm::scale(glm::mat4(1), glm::vec3(0.2f));
  glm::mat4 model_light = translate_light * scale_light;

  // spheres
  const unsigned int N_SPHERES = 3;
  glm::vec3 positions_sphere[N_SPHERES] = {
    glm::vec3( 7.0f, 1.5f, 6.0f),
    glm::vec3(16.0f, 1.5f, 6.0f),
    glm::vec3(26.5f, 1.5f, 6.0f),
  };

  ////////////////////////////////////////////////
  // Game loop
  ////////////////////////////////////////////////

  while (!window.is_closed()) {
    // clear color & depth buffers before rendering every frame
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_TRUE);

    // draw xyz gizmo at origin using GL_LINES
    gizmo.set_transform({ { glm::mat4(1.0) }, view, projection3d });
    gizmo.draw_lines({ {"colors[0]", glm::vec3(1.0f, 0.0f, 0.0f)} }, 2, 0);
    gizmo.draw_lines({ {"colors[0]", glm::vec3(0.0f, 1.0f, 0.0f)} }, 2, 2);
    gizmo.draw_lines({ {"colors[0]", glm::vec3(0.0f, 0.0f, 1.0f)} }, 2, 4);

    // draw horizontal 2d grid using GL_LINES
    grid.set_transform({ { glm::mat4(1.0) }, view, projection3d });
    grid.draw_lines({ {"colors[0]", glm::vec3(1.0f, 1.0f, 1.0f)} });

    // light cubes
    Transformation transform_cube({ model_light }, view, projection3d);
    cubes.set_transform(transform_cube);
    cubes.draw({
        {"colors", light.color}
    });

    // TODO: rotate in vertex shader & move inverseTranspose outside game loop
    // shaded sphere rotating around light
    // https://stackoverflow.com/a/53765106/2228912
    std::vector<glm::mat4> models_spheres(N_SPHERES), normals_mats_spheres(N_SPHERES);
    std::vector<glm::vec3> lights_positions(N_SPHERES), lights_ambiant(N_SPHERES), lights_diffuse(N_SPHERES), lights_specular(N_SPHERES);

    for (size_t i_sphere = 0; i_sphere < N_SPHERES; ++i_sphere) {
      glm::vec3 pivot = light.position;
      glm::mat4 model_sphere(glm::scale(
        glm::translate(
          glm::translate(
            glm::rotate(
              glm::translate(glm::mat4(1.0f), pivot), // moving pivot to its original pos.
              static_cast<float>(glfwGetTime()),
              glm::vec3(0.0f, 1.0f, 0.0f)
            ),
            -pivot // bringing pivot to origin first
          ),
          positions_sphere[i_sphere] // initial position (also makes radius smaller)
        ),
        glm::vec3(1)
      ));
      models_spheres[i_sphere] = model_sphere;

      // normal vec to world space (when non-uniform scaling): https://learnopengl.com/Lighting/Basic-Lighting
      glm::mat4 normal_mat = glm::inverseTranspose(model_sphere);
      normals_mats_spheres[i_sphere] = normal_mat;

      lights_positions[i_sphere] = light.position;
      lights_ambiant[i_sphere] = light.ambiant;
      lights_diffuse[i_sphere] = light.diffuse;
      lights_specular[i_sphere] = light.specular;

    } // SPHERES UNIFORMS ARRAYS

    Transformation transform_sphere({ models_spheres, view, projection3d });
    spheres.set_transform(transform_sphere);
    spheres.set_uniform_arr("normals_mats", normals_mats_spheres);
    spheres.set_uniform_arr("lights.position", lights_positions);
    spheres.set_uniform_arr("lights.ambiant", lights_ambiant);
    spheres.set_uniform_arr("lights.diffuse", lights_diffuse);
    spheres.set_uniform_arr("lights.specular", lights_specular);

    spheres.draw({
      {"material.ambiant", glm::vec3(1.0f, 0.5f, 0.31f)},
      {"material.diffuse", glm::vec3(1.0f, 0.5f, 0.31f)},
      {"material.specular", glm::vec3(0.5f, 0.5f, 0.5f)},
      // {"material.shininess", 32.0f},
      {"material.shininess", 4.0f}, // bigger specular reflection

      {"position_camera", camera.position},
    });

    // process events & show rendered buffer
    window.process_events();
    window.render();

    // leave main loop on press on <q>
    if (window.is_key_pressed(GLFW_KEY_Q))
      break;
  }

  // destroy shaders
  program_basic.free();
  program_phong.free();

  // destroy renderers of each shape (frees vao & vbo)
  gizmo.free();
  grid.free();
  cubes.free();
  spheres.free();

  // destroy window & terminate glfw
  window.destroy();

  return 0;
}
