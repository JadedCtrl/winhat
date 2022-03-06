void	main(int argc, char** argv);
void	usage(void);

void	monitor_window(int winId);
int	check_window(const char* path, int rect[4], int* focus);

void	focus_window(const char* path);

int	resize_own_window(int rect[4]);
void	draw_own_window(void);

int	wctl_accessibility(const char* path, int mode);
int	wctl_path(int winId, char* buff);

int	read_line(int wctl_fd, char* buf);
int	analyze_column(char* buf, int column);
