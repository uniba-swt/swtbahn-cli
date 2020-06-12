class BahnAPI {

    constructor(baseUrl = "") {
        this.baseUrl = baseUrl;
    }

    async login(username, password) {
        this.username = username;
        this.password = password;

        // ping server to check if the provided credentials are correct
        let response = await this._fetch('/userinfo');
        let userInfo = await response.json();

        console.log(userInfo);

        return userInfo.loggedIn;
    }

    logout() {
        this.username = null;
        this.password = null;
    }

    async ping() {
        await this._fetch('/');
    }

    async startup() {
        await this._fetch('/admin/startup', 'POST');
    }

    async shutdown() {
        await this._fetch('/admin/shutdown', 'POST');
    }

    async _fetch(path, method = 'GET', body = null) {
        let headers = new Headers();

        if (this.username && this.password) {
            headers.set('Authorization', 'Basic ' + window.btoa(this.username + ':' + this.password));
        }

        return fetch(this.baseUrl + path, {
            method: method,
            headers: headers,
            body: body
        })
    }
}