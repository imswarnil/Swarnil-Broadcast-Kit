# Every routine job in this repository, in one place.
#
# There is no package.json and no task runner to install: the site is plain Node
# with no dependencies, the plugin is CMake, and this is a list of the commands
# you would otherwise have to remember. `make` on its own prints it.

SHELL := /bin/bash
.DEFAULT_GOAL := help

.PHONY: help build install site check skill presets selftest shots clean release

help: ## Show this list
	@grep -hE '^[a-z-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN{FS=":.*?## "}{printf "  \033[1m%-10s\033[0m %s\n", $$1, $$2}'

build: ## Compile the plugin (does not install it)
	cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake --build build

install: ## Build and install the plugin, fonts and profile — quit OBS first
	./build.command

site: ## Build the documentation site into dist/
	node site/build.mjs

skill: ## Regenerate the agent skill's registry from the C
	node scripts/skill-sync.mjs

check: site ## Everything CI runs, in the order CI runs it
	node site/check.mjs
	node scripts/skill-sync.mjs --check
	node scripts/check-recipes.mjs

presets: ## Build the ready-made collections into dist/presets/
	node site/build.mjs
	@ls -lh dist/presets/

selftest: ## Arm the clean self-test; start OBS afterwards
	@touch "$$HOME/Library/Application Support/obs-studio/.sbk-selftest-clean"
	@echo "armed — start OBS. Screenshots land in the profile's recording folder."

shots: ## Copy the newest self-test run into docs/screens/
	@node scripts/import-shots.mjs

release: check ## Tag the version in CMakeLists and push it
	@v=$$(sed -n 's/.*project(sbk VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt); \
	 git diff --quiet || { echo "working tree is dirty"; exit 1; }; \
	 echo "tagging v$$v"; git tag -a "v$$v" -m "Swarnil Broadcast Kit $$v" && git push --follow-tags

clean: ## Remove build output
	rm -rf build dist
