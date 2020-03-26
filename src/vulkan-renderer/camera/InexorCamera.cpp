#include "InexorCamera.hpp"


namespace inexor {
namespace vulkan_renderer {


	InexorCamera::InexorCamera()
	{
	}

	
	InexorCamera::~InexorCamera()
	{
	}

	
	void InexorCamera::set_position(const glm::vec3& position)
	{
		this->position = position;
	}


	void InexorCamera::set_direction(const glm::vec3& direction)
	{
		this->direction = direction;
	}


	glm::vec3 InexorCamera::get_direction() const
	{
		return this->direction;
	}


	void InexorCamera::start_camera_movement(bool moveing_backwards)
	{
		this->camera_is_moving = true;
		this->moving_backwards = moveing_backwards;
	}


	void InexorCamera::end_camera_movement()
	{
		this->camera_is_moving = false;
		this->moving_backwards = false;
	}


	void InexorCamera::update()
	{
		if(camera_is_moving)
		{
			if(!moving_backwards)
			{
				move_forwards();
			}
			else
			{
				move_backwards();
			}
		}
	}


	void InexorCamera::move_forwards()
	{
		auto time_passed_factor = timestep.get_time_step();
		this->position += (camera_speed * time_passed_factor * direction);
	}


	void InexorCamera::move_backwards()
	{
		auto time_passed_factor = timestep.get_time_step();
		this->position -= (camera_speed * time_passed_factor * direction);
	}

	
	glm::vec3 InexorCamera::get_position() const
	{
		return this->position;
	}


	void InexorCamera::set_yaw(const float yaw)
	{
		this->yaw = yaw;
	}

			
	void InexorCamera::set_pitch(const float pitch)
	{
		this->pitch = pitch;
	}


	void InexorCamera::set_roll(const float roll)
	{
		this->roll = roll;
	}
	

	float InexorCamera::get_yaw() const
	{
		return this->yaw;
	}


	float InexorCamera::get_pitch() const
	{
		return this->pitch;
	}


	float InexorCamera::get_roll() const
	{
		return this->roll;
	}


	void InexorCamera::set_rotation(const float yaw, const float pitch, const float roll)
	{
		this->yaw   = yaw;
		this->pitch = pitch;
		this->roll  = roll;
	}


	void InexorCamera::move_camera_x(const float x)
	{
		auto time_passed_factor = timestep.get_time_step();
		this->position.x += camera_speed * time_passed_factor * x;
	}


	void InexorCamera::move_camera_y(const float y)
	{
		auto time_passed_factor = timestep.get_time_step();
		this->position.y += camera_speed * time_passed_factor * y;
	}


	void InexorCamera::move_camera_z(const float z)
	{
		auto time_passed_factor = timestep.get_time_step();
		this->position.z += camera_speed * time_passed_factor * z;
	}


	void InexorCamera::set_speed(const float camera_speed)
	{
		assert(camera_speed > 0.0f);
		this->camera_speed = camera_speed;
	}

	
	float InexorCamera::get_speed() const
	{
		return this->camera_speed;
	}


	void InexorCamera::set_near_plane(const float near_plane)
	{
		assert(near_plane > 0.0f);
		this->near_plane = near_plane;
	}


	float InexorCamera::get_near_plane() const
	{
		return this->near_plane;
	}


	void InexorCamera::set_far_plane(const float far_plane)
	{
		assert(far_plane > 0.0f);
		this->far_plane = far_plane;
	}


	float InexorCamera::get_far_plane() const
	{
		return this->far_plane;
	}


	void InexorCamera::set_zoom(const float zoom)
	{
		assert(zoom > 0.0f);

		this->zoom = zoom;
	}


	float InexorCamera::get_zoom() const
	{
		return zoom;
	}
	

	void InexorCamera::set_aspect_ratio(const float aspect_ratio)
	{
		assert(aspect_ratio > 0.0f);
		this->aspect_ratio = aspect_ratio;
	}
			

	float InexorCamera::get_aspect_ratio() const
	{
		return this->aspect_ratio;
	}


	glm::mat4 InexorCamera::get_view_matrix()
	{
		// TODO: Update only when data has changed!
		return glm::lookAt(direction, position, world_up);
	}


	glm::mat4 InexorCamera::get_projection_matrix()
	{
		// TODO: Update only when data has changed!
		return glm::perspective(glm::radians(45.0f), aspect_ratio, near_plane, far_plane);
	}


	glm::vec3 InexorCamera::get_up() const
	{
		return this->world_up;
	}
	
	
	glm::vec3 InexorCamera::get_front() const
	{
		return this->world_front;
	}
	
	
	glm::vec3 InexorCamera::get_right() const
	{
		return this->world_right;
	}


};
};