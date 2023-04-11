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
  glm::vec3 position_cam(0, 10, 20);
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
  glm::vec3 position_light(0, 5, 0);
  Light light(position_light, glm::vec3(1));
  glm::mat4 translate_light = glm::translate(glm::mat4(1), light.position);
  glm::mat4 scale_light = glm::scale(glm::mat4(1), glm::vec3(0.2f));
  glm::mat4 model_light = translate_light * scale_light;

  // spheres
  glm::vec3 light_position = light.position;
  glm::vec3 light_ambiant = light.ambiant;
  glm::vec3 light_diffuse = light.diffuse;
  glm::vec3 light_specular = light.specular;

  ////////////////////////////////////////////////
  // Pendulum physics
  ////////////////////////////////////////////////

  /**
   * References:
   * Coding Train video: https://www.youtube.com/watch?v=NBWMtlbbOag
   * Equations: https://en.wikipedia.org/wiki/Pendulum_(mechanics)
   * Equations: http://calculuslab.deltacollege.edu/ODE/7-A-2/7-A-2-h.html
   */

  // Newton's second law: F = m * acc = acc (supposedly, mass = 1)
  float gravity = 0.015;
  glm::vec3 pivot = position_light;
  float length = glm::length(pivot);

  // angle rel. to position at rest (vertical axis)
  float angle = M_PI / 3;
  float velocity_ang = 0,
        acceleration_ang = 0;


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

    /**
     * Movement of pendulum bob (i.e. sphere)
     */

    // linear or tangential acceleration
    // tangential component of gravitational force in opposite dir to angle (=> negative)
    // i.e. F_tan = -F * sin(theta) => acc_lin = -g * sin(theta) (mass=1 supposedly)
    float acceleration_lin = -gravity * sin(angle);

    // angular acceleration
    // from 2nd derivative of: arc = length * theta => acc_ang = d^2(theta)/d^t2 = acc_lin / length
    float acceleration_ang = acceleration_lin / length;

    // update angular velocity using angular acceleration
    velocity_ang += acceleration_ang;
    angle += velocity_ang;

    // update sphere bob xy coords
    glm::vec3 position_sphere(
        pivot.x + length * std::sin(angle),
        pivot.y - length * std::cos(angle),
        0
    );

    /**
     * Render sphere
     */

    // normal vec to world space (when non-uniform scaling): https://learnopengl.com/Lighting/Basic-Lighting
    glm::mat4 model_sphere = glm::translate(glm::mat4(1), position_sphere);
    glm::mat4 normal_mat = glm::inverseTranspose(model_sphere);

    // spheres
    Transformation transform_sphere({ {model_sphere}, view, projection3d });
    spheres.set_transform(transform_sphere);

    spheres.draw({
      {"material.ambiant", glm::vec3(1.0f, 0.5f, 0.31f)},
      {"material.diffuse", glm::vec3(1.0f, 0.5f, 0.31f)},
      {"material.specular", glm::vec3(0.5f, 0.5f, 0.5f)},
      // {"material.shininess", 32.0f},
      {"material.shininess", 4.0f}, // bigger specular reflection

      {"normals_mats[0]", normal_mat},
      {"lights[0].position", light_position},
      {"lights[0].ambiant", light_ambiant},
      {"lights[0].diffuse", light_diffuse},
      {"lights[0].specular", light_specular},

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
