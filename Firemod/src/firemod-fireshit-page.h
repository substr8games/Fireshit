#ifndef FIREMOD_FIRESHIT_PAGE_H
#define FIREMOD_FIRESHIT_PAGE_H

#include "firemod-app.h"

typedef struct FiremodFireshitPage FiremodFireshitPage;

FiremodFireshitPage *firemod_fireshit_page_new(FiremodApp *app);
GtkWidget *firemod_fireshit_page_widget(FiremodFireshitPage *page);
void firemod_fireshit_page_refresh(FiremodFireshitPage *page);
void firemod_fireshit_page_free(FiremodFireshitPage *page);

#endif
