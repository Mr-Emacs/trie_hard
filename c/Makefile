CC := clang
LOG_DIR := log
OUT_DIR := report_dist
RUNS ?= 10
FILES := 15kb-word.txt 30kb-word.txt

RELEASE_FLAGS := -O3 -march=native -funroll-loops -fomit-frame-pointer
DEBUG_FLAGS := -g -O0 -DDEBUG
ARENA_FLAGS := -DTR_USE_ARENA

all: no-opt-debug no-opt-release full-opt-debug full-opt-release

define run_bench
	@mkdir -p $(LOG_DIR)
	$(CC) $(2) $(3) -o $(LOG_DIR)/$(1)
	@for f in $(FILES); do \
		echo "--- Benchmarking $(1) with $$f (N=$(RUNS)) ---"; \
		rm -f $(LOG_DIR)/$(1)-$$f.log; \
		for i in $$(seq 1 $(RUNS)); do $(LOG_DIR)/$(1) $$f >> $(LOG_DIR)/$(1)-$$f.log; done; \
	done
endef

no-opt-debug:   ; $(call run_bench,$@,no-opt-src/bench.c,$(DEBUG_FLAGS))
no-opt-release: ; $(call run_bench,$@,no-opt-src/bench.c,$(RELEASE_FLAGS))
full-opt-debug: ; $(call run_bench,$@,full-opt-src/bench.c,$(ARENA_FLAGS) $(DEBUG_FLAGS))
full-opt-release: ; $(call run_bench,$@,full-opt-src/bench.c,$(ARENA_FLAGS) $(RELEASE_FLAGS))

report: all
	@mkdir -p $(OUT_DIR)
	python3 report.py $(RUNS)
	gnuplot plot.gp
	@mv summary_plot.png $(OUT_DIR)/
	@mv report.html $(OUT_DIR)/
	@rm -f plot_data.dat plot.gp
	@echo "\n[Success] Multi-file dashboard ready in: $(OUT_DIR)/"

clean:
	rm -rf $(LOG_DIR) $(OUT_DIR)
