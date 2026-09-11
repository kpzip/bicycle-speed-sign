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

std::vector<SDL_FPoint> scale(const std::vector<radar_data_t> data) {
	std::vector<SDL_FPoint> pts;
	pts.reserve(data.size());
	for(size_t i = 0; i < data.size(); i++)
		pts.emplace_back((float)i/(data.size()-1), data[i] / 2);
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

	std::jthread receive_thread{receive_task};

	bool run = true;
	while(run) {
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if(event.type == SDL_EVENT_QUIT)
				run = false;
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
