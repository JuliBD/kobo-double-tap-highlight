#ifndef DTP_REPORT_H
#define DTP_REPORT_H
#ifdef __cplusplus
extern "C" {
#endif
/* Opens/closes for each record; total file size is bounded at 8 KiB. */
void dtp_append_report(const char *path, const char *kind);
/* GUI-thread diagnostic log: retain newest records in two files, each <=8 KiB. */
void dtp_append_recent_report(const char *path, const char *kind);
#ifdef __cplusplus
}
#endif
#endif
