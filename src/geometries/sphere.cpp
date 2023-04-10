#include <iostream>
#include <cmath>

#include "geometries/sphere.hpp"

using namespace geometry;

Sphere::Sphere(float radius, unsigned int n_longitudes, unsigned int n_latitudes):
  m_radius(radius),
  m_n_longitudes(n_longitudes),
  m_n_latitudes(n_latitudes),

  // one at beginning & end corresp. to vertexes at north/south poles resp.
  m_n_vertexes(1 + m_n_longitudes * (m_n_latitudes - 1) + 1)
{
  // reserve space for position & normal coords for every vertex
  m_vertexes.resize(m_n_coords * m_n_vertexes);

  // calculate vertexes (xyz & normals) & indices on creation
  set_vertexes();
  set_indices();
  set_normals();
}

/**
 * Sets vertexes for sphere of given radius
 * Parametric equation for a sphere depends on lon (theta) & lat (phi) angles
 * See spherical coords: https://mathinsight.org/spherical_coordinates
 */
void Sphere::set_vertexes() {
  // north-pole
  m_vertexes[0] = 0.0f;
  m_vertexes[1] = m_radius;
  m_vertexes[2] = 0.0f;

  // vertexes belonging to horizontal circles between north & south poles
  // angles lon in [0, 2pi] rad and lat in [0, pi] rad
  float lon_step = 2*M_PI / m_n_longitudes;
  float lat_step = M_PI / m_n_latitudes;

  // angles rel. to vertical y-axis (phi angle) - start at north-pole & go downwards
  unsigned int i_coord = m_n_coords;
  for (size_t i_lat = 1; i_lat < m_n_latitudes; i_lat++) {
    float lat = i_lat * lat_step;

    // angles rel. to x-axis (theta angle) - start at x-axis & rotate ccw
    for (size_t i_lon = 0; i_lon < m_n_longitudes; i_lon++) {
      float lon = i_lon * lon_step;

      m_vertexes[i_coord] = m_radius * std::cos(lon) * std::sin(lat);
      m_vertexes[i_coord + 1] = m_radius * std::cos(lat);
      m_vertexes[i_coord + 2] = m_radius * std::sin(lon) * std::sin(lat);
      i_coord += m_n_coords;
    }
  }

  // south-pole
  m_vertexes[i_coord] = 0.0f;
  m_vertexes[i_coord + 1] = -m_radius;
  m_vertexes[i_coord + 2] = 0.0f;

  std::cout << "# of vertexes coords: " << m_vertexes.size() << '\n';
}

/* Proceed from north-pole downwards towards south-pole */
void Sphere::set_indices() {
  // triangles from north-pole to horizontal circle just below it
  for (unsigned int i_lon = 1; i_lon < m_n_longitudes; i_lon++) {
    m_indices.insert(m_indices.end(), { 0, i_lon + 1, i_lon });
  }

  // close cycle at very top with last triangle
  m_indices.insert(m_indices.end(), { 0, 1, m_n_longitudes });


  // start from first horizontal circle below south-pole & draw triangle between it & following one
  /*
     p1 p0
     x--x
     |\ |
     | \|
     x--x
     p3 p2
  */
  for (unsigned int i_lat = 0; i_lat < m_n_latitudes - 2; i_lat++) {
    for (unsigned int i_lon = 1; i_lon < m_n_longitudes; i_lon++) {
      unsigned int i_p0 = i_lon                    + i_lat*m_n_longitudes;
      unsigned int i_p1 = i_p0 + 1;
      unsigned int i_p2 = (i_lon + m_n_longitudes) + i_lat*m_n_longitudes;
      unsigned int i_p3 = i_p2 + 1;

      // two triangles in same face
      m_indices.insert(m_indices.end(), { i_p0, i_p1, i_p2 });
      m_indices.insert(m_indices.end(), { i_p1, i_p3, i_p2 });
    }

    // close cycle with last face
    unsigned int i_p0 = 1              + i_lat*m_n_longitudes;
    unsigned int i_p1 = m_n_longitudes + i_lat*m_n_longitudes;
    unsigned int i_p2 = i_p0 + m_n_longitudes;
    unsigned int i_p3 = i_p1 + m_n_longitudes;

    m_indices.insert(m_indices.end(), { i_p0, i_p2, i_p1 });
    m_indices.insert(m_indices.end(), { i_p1, i_p2, i_p3 });
  }


  // triangles from second-to-last horizontal circle to south-pole
  unsigned int indice_last = m_n_longitudes * (m_n_latitudes - 1) + 1;
  for (unsigned int i_lon = 1; i_lon < m_n_longitudes; i_lon++) {
    m_indices.insert(m_indices.end(), { indice_last, indice_last - (i_lon + 1), indice_last - i_lon });
  }

  // close cycle at very bottom with last triangle
  m_indices.insert(m_indices.end(), { indice_last, indice_last - 1, indice_last - m_n_longitudes });
}

/**
 * The normal at a vertex on the sphere surface is the vector from center (origin) to that vertex
 * https://stackoverflow.com/a/8024926/2228912
 */
void Sphere::set_normals() {
  for (size_t i_vertex = 0; i_vertex < m_n_vertexes; i_vertex++) {
    // normals stored after xyz positions (normalize as not always unit sphere)
    unsigned int i_coord = m_n_coords * i_vertex;
    glm::vec3 normal(m_vertexes[i_coord], m_vertexes[i_coord + 1], m_vertexes[i_coord + 2]);
    normal = glm::normalize(normal);

    m_vertexes[i_coord + 3] = normal.x;
    m_vertexes[i_coord + 4] = normal.y;
    m_vertexes[i_coord + 5] = normal.z;
  }
}

std::vector<float> Sphere::get_vertexes() const {
  return m_vertexes;
}

unsigned int Sphere::get_n_elements() const {
  return m_indices.size();
}
