CC := gcc
LIBS := -lreadline -lm
TARGET_EXEC := jsh
BUILD_DIR := ./build
SRC_DIRS := ./src

# On récupère les fichiers .c pour les rediriger une fois compilés.
SRCS := $(shell find $(SRC_DIRS) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.c.o=.o)

CYAN := \033[0;36m
NC := \033[0m

.PHONY : clean run

$(TARGET_EXEC): $(DEPS)
	@$(CC) $(DEPS) -o $@ $(LIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) -c $< -o $@


clean : 
	@rm -rf $(BUILD_DIR)/ $(TARGET_EXEC)

run : $(TARGET_EXEC)
	@echo "$(CYAN)Building project..."
	@echo "Running jsh...$(NC)"
	@./$(TARGET_EXEC)

