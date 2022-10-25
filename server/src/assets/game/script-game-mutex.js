/**
 * Simple global mutex to protect the execution of asynchronous code.
 * Mutex requests are handled in a first-come, first-served basis.
 *
 * Example usage:
 *

function resolveAfterXSeconds(timeout, msg) {
	return new Promise(resolve => {
		setTimeout((timeout) => {
			resolve(msg + ' resolved');
		}, timeout*1000);
	});
}

async function asyncCall(timeout, msg) {
	const request = await Mutex.lock();
	
	console.log(msg + ' calling');
	const result = await resolveAfterXSeconds(timeout, msg);
	console.log(result);
	
	Mutex.unlock(request);
}

for (let i = 0; i < 2; i++) {
	asyncCall(3, '\t\t three');
	asyncCall(2, '\t two');
	asyncCall(1, 'one');
}

*
**/

// A mutex request begins as an unresolved promise, along with 
// its resolve and reject functions.
class MutexRequest {
	promise = null;
	resolve = null;
	reject = null;
	
	constructor() {
		this.promise = new Promise((resolveP, rejectP) => {
			this.resolve = resolveP;   // Save the resolve and reject functions 
			this.reject = rejectP;     // defined by the promise constructor.
		});
	}
}

// Mutex requests are handled in a first-come, first-served basis.
// An ordered queue of lock requests is maintained:
// * lock(): a new request is enqueued
// * unlock(request): request is dequeued
// To guarantee mutual exclusion, lock() waits for all prior requests 
// to be unlocked before granting the current request.
class Mutex {
	static requests = [];
	
	constructor() {}
	
	// Asynchronously grants the lock request. Caller must `await` 
	// for the grant before entering its critical section.
	static async lock() {
		const priorRequest = Mutex.requests.at(-1);
		
		const newRequest = new MutexRequest();
		Mutex.requests.push(newRequest);
				
		if (priorRequest != undefined) {
			await priorRequest.promise;
		}

		return newRequest;
	}
	
	// Synchronously releases the granted lock request by resolving 
	// the request.
	static unlock(request) {
		const index = Mutex.requests.findIndex(
			(item) => (item.promise === request.promise)
		);
		
		if (index == 0) {
			request.resolve();
			Mutex.requests.splice(index, 1);   // Dequeue the request.
		} else if (index == -1) {
			request.reject('Mutex.unlock: request could not be found');
		} else {
			request.reject('Mutex.unlock: prior requests are still unresolved');
		}
	}
}
