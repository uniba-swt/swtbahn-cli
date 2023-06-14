class Timer {
	sec = 0;
	min = 0;
	timerInterval;

	tick() {
		this.sec++;
		if (this.sec == 60) {
			this.sec = 0;
			this.min++;
		}
		var minString = this.min.toString().padStart(2, '0');
		var secString = this.sec.toString().padStart(2, '0');
		$("#timerMinute").html(minString);
		$("#timerSecond").html(secString);
	}

	start() {
		this.sec = 0;
		this.min = 0;
		this.timerInterval = setInterval(() => { 
			this.tick(); 
		}, 1000);
	}

	stop() {
		clearInterval(this.timerInterval);
		this.sec = 0;
		this.min = 0;
	}
}
