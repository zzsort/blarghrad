/*
===========================================================================
Copyright (C) 1997-2006 Id Software, Inc.

This file is part of Quake 2 Tools source code.

Quake 2 Tools source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake 2 Tools source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake 2 Tools source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "pch.h"
#include "cmdlib.h"
#include "threads.h"

#define	MAX_THREADS	64

int		dispatch;
int		workcount;
int		oldf;
qboolean		pacifier;

qboolean	threaded;

/*
=============
GetThreadWork

=============
*/
int	GetThreadWork (void)
{
	int	r;
	int	f;

    std::lock_guard<std::mutex> lock(threadMutex);

	if (dispatch == workcount)
	{
		return -1;
	}

    f = ((dispatch * 5 + 5) * 8) / workcount;
	r = dispatch;
    while (f - 1U != oldf) {
        oldf++;
        if (pacifier == 0)
            continue;

        /*if (f != oldf)
        {
            oldf = f;
            if (pacifier)
                printf("%i...", f);
        }*/

        if ((oldf % 4) == 0) {
            dispatch = r;
            printf("%i", (int)(oldf + ((int)oldf >> 0x1f & 3U)) >> 2); // TODO fix expr
            fflush(stdout);
            r = dispatch;
        }
        else {
            dispatch = r;
            printf(".");
            fflush(stdout);
            r = dispatch;
        }
    }

	dispatch++;

	return r;
}


void (*workfunction) (int);

void ThreadWorkerFunction (int threadnum)
{
	int		work;

	while (1)
	{
		work = GetThreadWork ();
		if (work == -1)
			break;
//printf ("thread %i, work %i\n", threadnum, work);
		workfunction(work);
	}
}

void RunThreadsOnIndividual (int workcnt, qboolean showpacifier, void(*func)(int))
{
	if (numthreads == -1)
		ThreadSetDefault ();
	workfunction = func;
	RunThreadsOn (workcnt, showpacifier, ThreadWorkerFunction);
}

#define USED

int		numthreads = -1;
std::mutex threadMutex;

void ThreadSetDefault (void)
{
	if (numthreads == -1)	// not set manually
	{
		numthreads = (int)std::thread::hardware_concurrency();
		if (numthreads < 1 || numthreads > 32)
			numthreads = 1;
	}

	qprintf ("%i threads\n", numthreads);
}

/*
=============
RunThreadsOn
=============
*/
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(int))
{
	int		i;
	int		start, end;

    if (numthreads == -1) {
        ThreadSetDefault();
    }

    workfunction = func;
	start = I_FloatTime ();
	dispatch = 0;
	workcount = workcnt;
	oldf = -1;
	pacifier = showpacifier;

	//
	// run threads in parallel
	//
	if (numthreads == 1)
	{	// use same thread
        ThreadWorkerFunction(0); // func(0);
	}
	else
    {
	    threaded = true;
        std::vector<std::thread> threads;
		for (i=0 ; i<numthreads ; i++) {
            threads.emplace_back(std::thread(ThreadWorkerFunction, i));
		}

        for (auto& t : threads) {
            t.join();
        }
	    threaded = false;
	}

	end = I_FloatTime ();
	if (pacifier)
		printf (" (%i)\n", end-start);
}


/*
=======================================================================

  SINGLE THREAD

=======================================================================
*/

#ifndef USED

void ThreadSetDefault (void)
{
	numthreads = 1;
}

/*
=============
RunThreadsOn
=============
*/
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(int))
{
	int		i;
	int		start, end;

	dispatch = 0;
	workcount = workcnt;
	oldf = -1;
	pacifier = showpacifier;
	start = I_FloatTime ();
#ifdef NeXT
	if (pacifier)
		setbuf (stdout, NULL);
#endif
	func(0);

	end = I_FloatTime ();
	if (pacifier)
		printf (" (%i)\n", end-start);
}

#endif
