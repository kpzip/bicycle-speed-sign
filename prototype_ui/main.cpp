#include "receive.hpp"

#include <SDL3/SDL.h>

#include <thread>
#include <algorithm>
#include <vector>

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

SDL_FColor color_map(float value) {
	return {value, 0.0, 1.0, 1.0};
}

radar_data_t max = 0.1;

int avgs = 0;
const int avgs_count = 5;
std::vector<radar_data_t> avg;

bool log_scale = false;

std::vector<SDL_FPoint> scale(std::vector<radar_data_t> data) {
	// Average readings
	if(avgs) {
		if(avg.size() != data.size()) {
			avgs = avgs_count;
			avg = data;
		} else for(size_t i = 0; i < data.size(); i++)
			avg[i] += data[i];
		if(!--avgs) for(auto &x:avg)
			x /= avgs_count;
	}
	
	// Subtract average
	else if(avg.size() == data.size())
		for(size_t i = 0; i < data.size(); i++)
			data[i] -= avg[i];
	
	// Normalize
	for(auto &x:data)
		if(x > max) max = x;
	
	for(auto &x:data)
		x /= max;
	
	// Log scale
	if(log_scale)
		for(auto &x:data) {
			if(x < 0) x = 1e-9;
			x = std::log10(x);
			if(std::isnan(x) || std::isinf(x))
				x = 0; // source moment
			x += 2;
			x /= 2;
			x = std::max(0.0f, std::min(1.0f, x));
		}
	
	std::vector<SDL_FPoint> pts;
	pts.reserve(data.size());
	for(size_t i = 0; i < data.size(); i++)
		pts.emplace_back((float)i/(data.size()-1), data[i]);
	return pts;
}

std::vector<SDL_Vertex> gen_verts(const std::vector<SDL_FPoint> pts) {
	if(pts.size() < 2) return {};
	std::vector<SDL_Vertex> verts;
	verts.reserve((pts.size()-1)*6);
	for(size_t i = 0; i < pts.size()-1; i++) {
		auto &pt1 = pts[i];
		auto &pt2 = pts[i+1];
		auto color = color_map(pt1.y);

		verts.emplace_back(SDL_Vertex{{pt1.x, 1}, color, {}});
		verts.emplace_back(SDL_Vertex{{pt1.x, 1-pt1.y}, color, {}});
		verts.emplace_back(SDL_Vertex{{pt2.x, 1}, color, {}});
		verts.emplace_back(SDL_Vertex{{pt2.x, 1}, color, {}});
		verts.emplace_back(SDL_Vertex{{pt2.x, 1-pt2.y}, color, {}});
		verts.emplace_back(SDL_Vertex{{pt1.x, 1-pt1.y}, color, {}});
	}
	return verts;
}

int main() {
	if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
		return 1;

	if(!SDL_CreateWindowAndRenderer("radar", 640, 480, SDL_WINDOW_RESIZABLE, &window, &renderer))
		return 1;

	SDL_SetRenderLogicalPresentation(renderer, 1, 1, SDL_LOGICAL_PRESENTATION_STRETCH);
	SDL_SetRenderVSync(renderer, 1);
	SDL_HideCursor();

	std::jthread receive_thread{receive_task};

	bool run = true;
	while(run) {
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_EVENT_QUIT:
					run = false;
					break;
				case SDL_EVENT_KEY_DOWN:
					switch(event.key.key) {
						case SDLK_Q:
							run = false;
							break;
						case SDLK_S:
							max = 0.1;
							break;
						case SDLK_0:
							avgs = avgs_count;
							break;
						case SDLK_9:
							avg.clear();
							avgs = 0;
							break;
						case SDLK_L:
							log_scale ^= 1;
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);

		auto points = scale(give_data());
		auto verts = gen_verts(points);
		SDL_RenderGeometry(renderer, nullptr, verts.data(), verts.size(), nullptr, 0);

		SDL_RenderPresent(renderer);
	}

	SDL_Quit();
}
