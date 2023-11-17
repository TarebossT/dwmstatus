#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/Xlib.h>

char *tzbuc = "Europe/Bucharest";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	if (fd == NULL)
		return NULL;
	free(path);

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char *
getbattery(char *base)
{
    char *co, *status;
    int descap, remcap;

    descap = -1;
    remcap = -1;

    co = readfile(base, "present");
    if (co == NULL)
        return smprintf("");
    if (co[0] != '1') {
        free(co);
        return smprintf("not present");
    }
    free(co);

    co = readfile(base, "charge_full");
    if (co == NULL) {
        co = readfile(base, "energy_full");
        if (co == NULL)
            return smprintf("");
    }
    sscanf(co, "%d", &descap);
    free(co);

    co = readfile(base, "charge_now");
    if (co == NULL) {
        co = readfile(base, "energy_now");
        if (co == NULL)
            return smprintf("");
    }
    sscanf(co, "%d", &remcap);
    free(co);

    if (remcap < 0 || descap < 0)
        return smprintf("invalid");

    float capacity = ((float)remcap / (float)descap) * 100;
    char* bat_icon;
    if (capacity >= 90.0 && capacity <= 100.0) {
        bat_icon = "";
    } else if (capacity >= 60.0 && capacity < 90.0) {
        bat_icon = "";
    } else if (capacity >= 30.0 && capacity < 60.0) {
        bat_icon = "";
    } else if (capacity >= 25.0 && capacity < 30.0) {
        bat_icon = "";
    } else {
        bat_icon = "";
    }

    co = readfile(base, "status");
    if (!strncmp(co, "Discharging", 11)) {
        status = bat_icon;
    } else if (!strncmp(co, "Charging", 8)) {
        status = "";
    } else if (!strncmp(co, "Full", 4)) {
        status = "";
    } else {
        status = "";
    }

    return smprintf("%s%.0f%%",status, capacity);
}

char*
runcmd(char* cmd) {
	FILE* fp = popen(cmd, "r");
	if (fp == NULL) return NULL;
	char ln[30];
	fgets(ln, sizeof(ln)-1, fp);
	pclose(fp);
	ln[strlen(ln)-1]='\0';
	return smprintf("%s", ln);
}

char*
getvolume() {
	int volume;
  char mute;
  char *mute_str = "";
  sscanf(runcmd("amixer | grep -A 6 Master | grep 'Front Left: Playback'\
        | grep -o '[0-9%]*%'"), "%i%%", &volume);
  sscanf(runcmd("amixer | grep -A 6 Master | grep 'Front Left: Playback'\
        | grep -P -o '(\\[on)|(\\[of)'"), "[o%c", &mute);
  if (mute == 'f')
    mute_str = "(MUTE)";

	return smprintf("%d%%%s", volume, mute_str);
}

int
get_sensor_temp(char *base, char *sensor)
{
    char *co;

    co = readfile(base, sensor);
    if (co == NULL)
        return -1;

    return atoi(co) / 1000;
}

char *
getcputemp() {

    int cur_t, crit_t;
    char *icon;

    cur_t = get_sensor_temp("/sys/devices/virtual/thermal/thermal_zone0/hwmon1", "temp1_input");
    crit_t = get_sensor_temp("/sys/devices/virtual/thermal/thermal_zone0/hwmon1", "temp1_crit");

    float ratio = ((float)cur_t / (float)crit_t) * 100;

    if (ratio >= 90.0 && ratio <= 100.0) {
        icon = "";
    } else if (ratio >= 60.0 && ratio < 90.0) {
        icon = "";
    } else if (ratio >= 30.0 && ratio < 60.0) {
        icon = "";
    } else if (ratio >= 25.0 && ratio < 30.0) {
        icon = "";
    } else if (ratio >= 0.0 && ratio < 25.0 ) {
        icon = "";
    } else {
        icon = "";
    }

    return smprintf("%s%d°C", icon, cur_t);
}

char *
getbrightness(char *base)
{
        char *co;
        int max_brightness;
        int brightness;
        int percentage;

        co = readfile(base, "max_brightness");
        if (co == NULL)
                return 0;
        sscanf(co, "%d", &max_brightness);

        free(co);

        co = readfile(base, "brightness");
        if (co == NULL)
                return 0;
        sscanf(co, "%d", &brightness);
        
        percentage = brightness * 100 / max_brightness;

        return smprintf("%d%%", percentage);
}

char *
getmemstatus()
{
	FILE *pd;
	char buf[16];
	char cmd[64];

	snprintf(cmd, 64, "free -m | grep ^M | awk {'print ($3/$2)*100'}");

	pd = popen(cmd, "r");
	if (pd == NULL)
		return smprintf("");

	fgets(buf, 16, pd);

	pclose(pd);

	return smprintf("%d%%", atoi(buf));
}

char *
gethdd()
{
	FILE *pd;
	char buf[24];
	char cmd[64];

	snprintf(cmd, 64, "df -h | grep /media | awk '{print $4\"/\"$2 }'");

	pd = popen(cmd, "r");
	if (pd == NULL)
		return smprintf("");

	memset(buf, '\0', sizeof(buf));
	fgets(buf, 24, pd);
	int len = strlen(buf);
	if (len == 0)
		return smprintf("unmounted");
	buf[len-1] = '\0';

	pclose(pd);

	return smprintf("%s", buf);
}

char *
getwifistatus()
{
	FILE *pd;
	char buf[74];
	char cmd[94];

	snprintf(cmd, 94, "nmcli -g IN-USE,SSID,SIGNAL device wifi | awk '/^*/' | awk -F '[:]' '{print $2 \"/\" $3 \"%%\"}'");

	pd = popen(cmd, "r");
	if (pd == NULL)
		return smprintf("");

	memset(buf, '\0', sizeof(buf));
	fgets(buf, 74, pd);
	int len = strlen(buf);
	if (len == 0)
		return smprintf("not connected");
	buf[len-1] = '\0';

	pclose(pd);

	return smprintf("%s", buf);
}

char *
getcpuutil(void) {
    long double a[4], b[4], loadavg;
    FILE *fp;
    fp = fopen("/proc/stat", "r");
    fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
    fclose(fp);
    sleep(1);
    fp = fopen("/proc/stat", "r");
    fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
    fclose(fp);
    loadavg = ((b[0] + b[1] + b[2]) - (a[0] + a[1] + a[2])) / ((b[0] + b[1] + b[2] + b[3]) - (a[0] + a[1] + a[2] + a[3])) * 100;
    return smprintf("%.0Lf", loadavg);
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

int
main(void)
{
  char *status;
  char *wifi;
  char *hdd;
  char *mem;
  char *cpu;
  char *temp;
  char *bright;
  char *vol;
  char *bat;
  char *tmbuc;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(5)) {
    wifi = getwifistatus();
    hdd = gethdd();
    mem = getmemstatus();
    cpu = getcpuutil();	
    temp = getcputemp();
    bright = getbrightness("/sys/class/backlight/intel_backlight");
    vol = getvolume();
    bat = getbattery("/sys/class/power_supply/BAT0");
    tmbuc = mktimes("%c", tzbuc);

		status = smprintf("%s %s %s%% %s %s %s %s %s %s",
				  hdd, mem, cpu, temp, bright, vol, bat, tmbuc, wifi);
		setstatus(status);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}
