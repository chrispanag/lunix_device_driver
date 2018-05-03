/*
 * mk_lookup_tables.c
 *
 * Computes the temperature and battery
 * lookup tables for converting 16-bit raw measurements
 * from the wireless sensors to actual floating point values.
 *
 * Ioannis Panagopoulos <ioannis@cslab.ece.ntua.gr>
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 *
 */

#include <math.h>
#include <stdio.h>
#include <inttypes.h>

/*
 * Translates the received uint16_t value to voltage level
 */
long uint16_to_batt(uint16_t value)
{
	double d;

	if (value != 0)
		d =  1.223 * (1023.0 / value);
	else
		d = -0;
	
	return (long)(d * 1000);
}

/*
 * Translates the received uint16_t value to light level
 * (NOT YET IMPLEMENTED, just does a linear conversion)
 */
long uint16_to_light(uint16_t value)
{
	return (long) (value * 5000000.0 / 65535);
}

/*
 * Translates the received uint16_t value to temperature level
 */
long uint16_to_temp(uint16_t value)
{
	long l;

	double R1 = 10000.0;
	double ADC_FS = 1023.0;

	double Rth, Kelvin_Inv;

	double a = 0.001010024F;
	double b = 0.000242127F;
	double c = 0.000000146F;
	
	double res;
	 
	Rth = (R1 * (ADC_FS - (double)value)) / (double)value;
	Kelvin_Inv = a + b * log(Rth) + c * pow(log(Rth), 3);

	res = (1.0 / Kelvin_Inv) - 272.15;
	l = (long)(res * 1000);

	/* Useless values */
	return (l < -272150) ?  -272150 : l;
}

int main(void)
{
	unsigned int i;

	fprintf(stdout,
		"/*\n"
		" * lunix-tables.h\n"
		" *\n"
		" * Machine-generated file. DO NOT EDIT.\n"
		" * See %s instead.\n"
		" *\n"
		" * Instead of doing floating-point in kernelspace,\n"
		" * use the following lookup tables to convert 16-bit\n"
		" * raw measurements to floating point values.\n"
		" */\n"
		"\n"
		"long lookup_temperature[65536] = {\n", __FILE__);

	/*
	 * Temperature
	 */
	for (i = 0; i <= 0xFFFC; i += 4) {
		fprintf(stdout, "\t%ld, %ld, %ld, %ld",
			uint16_to_temp(i), uint16_to_temp(i+1),
			uint16_to_temp(i+2), uint16_to_temp(i+3));
		fprintf(stdout, (i != 0xFFFC) ? ",\n" : "\n");
	}

	fprintf(stdout, "};\n\n"
		"long lookup_voltage[65536] = {\n");

	/*
	 * Battery Voltage
	 */
	for (i = 0; i <= 0xFFFC; i += 4) {
		fprintf(stdout, "\t%ld, %ld, %ld, %ld",
			uint16_to_batt(i), uint16_to_batt(i+1),
			uint16_to_batt(i+2), uint16_to_batt(i+3));
		fprintf(stdout, (i != 0xFFFC) ? ",\n" : "\n");
	}
	fprintf(stdout, "};\n\n"
		"long lookup_light[65536] = {\n");

	/*
	 * Light
	 */
	for (i = 0; i <= 0xFFFC; i += 4) {
		fprintf(stdout, "\t%ld, %ld, %ld, %ld",
			uint16_to_light(i), uint16_to_light(i+1),
			uint16_to_light(i+2), uint16_to_light(i+3));
		fprintf(stdout, (i != 0xFFFC) ? ",\n" : "\n");
	}

	fprintf(stdout, "};\n\n");

	return 0;
}
