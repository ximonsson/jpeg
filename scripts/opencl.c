/**
 *	File: opencl.c
 *	Description: print platform information to stdout.
 * 			     compile with: gcc opencl.c -lOpenCL
 */

#include <CL/cl.h>
#include <stdio.h>


/**
 *  Check error code and return if it was success or not.
 *  Prints error message in case of error.
 *  Returns non-zero if error code is an error.
 */
static inline int check_error (cl_int error, const char* name)
{
    if (error != CL_SUCCESS)
    {
        printf ("error!\n");
        fprintf (stderr, "  CL error: %s (%d)\n", name, error);
        return 1;
    }
    return 0;
}


/**
 *  Print information to stdout about a platform from a id number.
 */
static void print_platform_info (cl_platform_id platform)
{
    char buffer[1024];
    cl_int error;

    error = clGetPlatformInfo (platform, CL_PLATFORM_NAME, 1024, &buffer, NULL);
    if (error == CL_SUCCESS)
        printf ("CL_PLATFORM_NAME : %s\n", buffer);

    error = clGetPlatformInfo (platform, CL_PLATFORM_VENDOR, 1024, &buffer, NULL);
    if (error == CL_SUCCESS)
        printf ("CL_PLATFORM_VENDOR : %s\n", buffer);

    error = clGetPlatformInfo (platform, CL_PLATFORM_VERSION, 1024, &buffer, NULL);
    if (error == CL_SUCCESS)
        printf ("CL_PLATFORM_VERSION: %s\n", buffer);

    error = clGetPlatformInfo (platform, CL_PLATFORM_PROFILE, 1024, &buffer, NULL);
    if (error == CL_SUCCESS)
        printf ("CL_PLATFORM_PROFILE: %s\n", buffer);

    error = clGetPlatformInfo (platform, CL_PLATFORM_EXTENSIONS, 1024, &buffer, NULL);
    if (error == CL_SUCCESS)
        printf ("CL_PLATFORM_EXTENSIONS: %s\n", buffer);

    return;
}




int main (int argc, char** argv)
{
	cl_int ret;
	cl_uint count;
	cl_platform_id platform_ids[2];

	ret = clGetPlatformIDs (2, platform_ids, &count);
    if (check_error (ret, "clGetPlatformIDs") != 0)
        return 1;

	for (int i = 0; i < count; i ++)
	{
		print_platform_info (platform_ids[i]);
		printf ("-------------------------------------------------------------------\n");
	}
	printf ("\n");

	return 0;
}
