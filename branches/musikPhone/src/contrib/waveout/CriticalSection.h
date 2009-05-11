#pragma once

class CriticalSectionX
{
protected:
	HANDLE	mutex;

public:
	CriticalSection()
	{
		this->mutex = CreateMutex(NULL, false, NULL);
	}

	~CriticalSection()
	{
		CloseHandle(this->mutex);
	}

	void Lock()
	{
		WaitForSingleObject(this->mutex, INFINITE);
	}

	void Unlock()
	{
		ReleaseMutex(this->mutex);
	}
};

class AutoLockX
{
private:
	CriticalSection *	CS;

public:

	AutoLock(CriticalSection *aCS)
	{
		this->CS = aCS;
		this->CS->Lock();
	}

	~AutoLock()
	{
		this->CS->Unlock();
	}
};
