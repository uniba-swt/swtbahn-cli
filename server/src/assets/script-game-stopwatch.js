class Stopwatch {
	elapsedTime = 0;       // milliseconds
	displayField = null;
	intervalId = null;
	updateInterval = 1000; // 1000 milliseconds
	
	constructor(htmlId) {
		this.displayField = $(htmlId);
		this.clear();
	}
	
	clear() {
		this.elapsedTime = 0;
		this.display();
	}
	
	start() {
		this.intervalId = setInterval(() => {
			this.increment();
			this.display();
		}, this.updateInterval);
	}
	
	increment() {
		this.elapsedTime += this.updateInterval;
	}
	
	stop() {
		clearInterval(this.intervalId);
		this.intervalId = null;
		this.display();
	}
	
	display() {
		this.displayField.text(this.elapsedTime/1000 + 's');
	}
}
