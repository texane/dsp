#ifndef UI_H_INCLUDED
# define UI_H_INCLUDED


int ui_init(unsigned int, unsigned int);
void ui_fini(void);
void ui_update_begin(void);
void ui_update_end(void);
void ui_update_ips(const double*, unsigned int);
void ui_update_ops(const double*, unsigned int);


#endif /* ! UI_H_INCLUDED */
