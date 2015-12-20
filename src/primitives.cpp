void generate_sphere_mesh(Mesh *mesh, float *radius_out, int bands) {
  float latitude_bands = bands;
  float longitude_bands = bands;
  float radius = 1.0;

  u32 vertices_count = (latitude_bands + 1) * (longitude_bands + 1) * 3;
  u32 normals_count = vertices_count;
  u32 uv_count = vertices_count;
  u32 indices_count = latitude_bands * longitude_bands * 6;

  allocate_mesh(mesh, vertices_count, normals_count, indices_count, uv_count, 0);

  u32 vertices_index = 0;
  u32 normals_index = 0;
  u32 uv_index = 0;
  u32 indices_index = 0;

  for (float lat_number = 0; lat_number <= latitude_bands; lat_number++) {
    float theta = lat_number * pi / latitude_bands;
    float sinTheta = sin(theta);
    float cos_theta = cos(theta);

    for (float long_number = 0; long_number <= longitude_bands; long_number++) {
      float phi = long_number * 2 * pi / longitude_bands;
      float sin_phi = sin(phi);
      float cos_phi = cos(phi);

      float x = cos_phi * sinTheta;
      float y = cos_theta;
      float z = sin_phi * sinTheta;

      mesh->data.uv[uv_index++] = 1 - (long_number / longitude_bands);
      mesh->data.uv[uv_index++] = 1 - (lat_number / latitude_bands);

      mesh->data.vertices[vertices_index++] = radius * x;
      mesh->data.vertices[vertices_index++] = radius * y;
      mesh->data.vertices[vertices_index++] = radius * z;

      mesh->data.normals[normals_index++] = x;
      mesh->data.normals[normals_index++] = y;
      mesh->data.normals[normals_index++] = z;
    }
  }

  for (int lat_number = 0; lat_number < latitude_bands; lat_number++) {
    for (int long_number = 0; long_number < longitude_bands; long_number++) {
      int first = (lat_number * (longitude_bands + 1)) + long_number;
      int second = first + longitude_bands + 1;

      mesh->data.indices[indices_index++] = first + 1;
      mesh->data.indices[indices_index++] = second;
      mesh->data.indices[indices_index++] = first;

      mesh->data.indices[indices_index++] = first + 1;
      mesh->data.indices[indices_index++] = second + 1;
      mesh->data.indices[indices_index++] = second;
    }
  }

  *radius_out = radius;
}
