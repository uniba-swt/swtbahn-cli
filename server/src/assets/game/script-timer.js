class Timer{
    sec = 0;
    min = 0;
    timerInterval;

    tick(){
        this.sec++;
        if (this.sec == 60){
	        this.sec = 0;
        	this.min++;
	}
    	var _min = ('0' + this.min.toString()).substr(-2);
    	var _sec = ('0' + this.sec.toString()).substr(-2);
    	$("#timerSecond").html(_sec);
    	$("#timerMinute").html(_min);
	console.log("Tick end");
    }


    start(){
	this.sec = 0;
	this.min = 0;
        this.timerInterval = setInterval(() => {this.tick();}, 1000);
    }
    stop(){
        clearInterval(this.timerInterval);
        this.sec = 0;
        this.min = 0;
    }
}
