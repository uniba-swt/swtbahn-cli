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
		var _min = this.min.toString().padStart(2, '0');
		var _sec = this.sec.toString().padStart(2, '0');
		$("#timerSecond").html(_sec);
		$("#timerMinute").html(_min);
	}

	start() {
		this.sec = 0;
		this.min = 0;
		this.timerInterval = setInterval(() => { this.tick(); }, 1000);
	}

	stop() {
		clearInterval(this.timerInterval);
		this.sec = 0;
		this.min = 0;
	}
}
